#include "render_graph_executor.hpp"

#include <stdexcept>
#include <utility>
#include <algorithm>

#include "../image/image_barrier.hpp"
#include "../engine.hpp"
#include "../utils.hpp"

#include "graph_utils.hpp"
#include "builder_resource.hpp"
#include "compiled_resource.hpp"
#include "render_graph_builder.hpp"

void RenderGraphExecutor::setCompiledGraph(CompiledRenderGraph _graph) {
    graph = _graph;
}

// ——— Images ——————————————————————————————————————————————————————————————————

void RenderGraphExecutor::initImages(VkSmol& engine) {
    for (uint32_t id = 0; id < graph.images.size(); ++id) {
        const ImageInfo& info = graph.images[id];
        if (info.ownership == ImageOwnership::Imported) continue;

        VkImageUsageFlags usageFlags = info.extraUsageFlags;
        for (const CompiledPass& pass : graph.compiledPasses)
            for (const ImageAccess& access : pass.imageAccesses)
                if (access.image.id == id)
                    usageFlags |= imageUsageTypeToVkFlags(access.info.usage);

        Image img = engine.createImage(
            info.width, info.height, info.format,
            usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, info.name
        );
        ImageView view = engine.createImageView(img, info.name + "View");
        Sampler   smp  = engine.createSampler(info.name + "Sampler");

        ImageHandle handle = { .id = id };
        bindImage(handle, img.get(), view.get());
        ownedImages.emplace(id, std::move(img));
        ownedViews.emplace(id, std::move(view));
        ownedSamplers.emplace(id, std::move(smp));
    }
}

void RenderGraphExecutor::destroyImages(VkSmol& engine) {
    for (auto& [id, img] : ownedImages) {
        engine.destroySampler(ownedSamplers.at(id));
        engine.destroyImageView(ownedViews.at(id));
        engine.destroyImage(img);
        bindings.erase(id);
    }
    ownedSamplers.clear();
    ownedViews.clear();
    ownedImages.clear();
}

// ——— Buffers —————————————————————————————————————————————————————————————————

void RenderGraphExecutor::initBuffers(VkSmol& engine) {
    for (uint32_t id = 0; id < graph.buffers.size(); ++id) {
        const BufferInfo& info = graph.buffers[id];
        if (info.ownership == BufferOwnership::Imported || info.perFrame) continue;

        VkBufferUsageFlags usageFlags = info.extraUsageFlags;
        for (const CompiledPass& pass : graph.compiledPasses)
            for (const BufferAccess& access : pass.bufferAccesses)
                if (access.buffer.id == id)
                    usageFlags |= bufferUsageTypeToVkFlags(access.info.usage);

        Buffer buf = info.initialData.empty()
            ? engine.createMappedBuffer(usageFlags, info.size, info.name)
            : engine.createBuffer(usageFlags, info.size, (void*)info.initialData.data(), info.name);
        bufferBindings[id] = buf.get();
        ownedBuffers.emplace(id, std::move(buf));
    }
}

void RenderGraphExecutor::destroyBuffers(VkSmol& engine) {
    for (auto& [id, buf] : ownedBuffers) {
        engine.destroyBuffer(buf);
        bufferBindings.erase(id);
    }
    ownedBuffers.clear();
}

void RenderGraphExecutor::initPerFrameBuffers(VkSmol& engine) {
    for (uint32_t id = 0; id < graph.buffers.size(); ++id) {
        const BufferInfo& info = graph.buffers[id];
        if (!info.perFrame || info.ownership == BufferOwnership::Imported) continue;

        VkBufferUsageFlags usageFlags = info.extraUsageFlags;
        for (const CompiledPass& pass : graph.compiledPasses)
            for (const BufferAccess& access : pass.bufferAccesses)
                if (access.buffer.id == id)
                    usageFlags |= bufferUsageTypeToVkFlags(access.info.usage);

        auto& arr = ownedPerFrameBuffers[id];
        for (uint32_t f = 0; f < MAX_FRAME_IN_FLIGHT; ++f)
            arr[f] = engine.createMappedBuffer(usageFlags, info.size, info.name + "[" + std::to_string(f) + "]");
    }
}

void RenderGraphExecutor::destroyPerFrameBuffers(VkSmol& engine) {
    for (auto& [id, arr] : ownedPerFrameBuffers)
        for (Buffer& buf : arr)
            engine.destroyBuffer(buf);
    ownedPerFrameBuffers.clear();
}

// ——— Sync ————————————————————————————————————————————————————————————————————

void RenderGraphExecutor::initSync(VkSmol& engine) {
    const uint32_t numGroups = static_cast<uint32_t>(graph.submissionGroups.size());

    imageAvailableSemaphores.resize(MAX_FRAME_IN_FLIGHT);
    groupSemaphores.resize(numGroups);
    groupFences.resize(numGroups);
    groupCommandBuffers.resize(numGroups);
    for (uint32_t g = 0; g < numGroups; ++g) {
        groupSemaphores[g].resize(MAX_FRAME_IN_FLIGHT);
        groupFences[g].resize(MAX_FRAME_IN_FLIGHT);
        groupCommandBuffers[g].resize(MAX_FRAME_IN_FLIGHT);
    }

    for (uint32_t f = 0; f < MAX_FRAME_IN_FLIGHT; ++f) {
        imageAvailableSemaphores[f].init(engine.getDevice());
        for (uint32_t g = 0; g < numGroups; ++g) {
            groupSemaphores[g][f].init(engine.getDevice());
            groupFences[g][f].init(engine.getDevice(), true);
            groupCommandBuffers[g][f] = engine.allocCommandBuffer();
        }
    }
}

void RenderGraphExecutor::destroySync(VkSmol& engine) {
    for (uint32_t f = 0; f < MAX_FRAME_IN_FLIGHT; ++f) {
        imageAvailableSemaphores[f].destroy(engine.getDevice());
        for (uint32_t g = 0; g < groupSemaphores.size(); ++g) {
            groupSemaphores[g][f].destroy(engine.getDevice());
            groupFences[g][f].destroy(engine.getDevice());
            engine.freeCommandBuffer(groupCommandBuffers[g][f]);
        }
    }
    imageAvailableSemaphores.clear();
    groupSemaphores.clear();
    groupFences.clear();
    groupCommandBuffers.clear();
}

// ——— Public lifecycle —————————————————————————————————————————————————————————

void RenderGraphExecutor::init(VkSmol& engine) {
    initImages(engine);
    initBuffers(engine);
    initPerFrameBuffers(engine);
    initSync(engine);
}

void RenderGraphExecutor::destroy(VkSmol& engine) {
    destroyPerFrameBuffers(engine);
    destroyBuffers(engine);
    destroyImages(engine);
    destroySync(engine);
}

void RenderGraphExecutor::resize(VkSmol& engine, uint32_t width, uint32_t height) {
    // Only images are resolution-dependent; sync objects stay intact
    destroyImages(engine);
    for (ImageInfo& info : graph.images) {
        if (info.ownership != ImageOwnership::Imported) {
            info.width  = width;
            info.height = height;
        }
    }
    initImages(engine);
}

// ——— Frame ————————————————————————————————————————————————————————————————————

std::optional<FrameContext> RenderGraphExecutor::beginFrame(VkSmol& engine) {
    std::vector<const Fence*> fences;
    for (auto& groupFenceRow : groupFences)
        fences.push_back(&groupFenceRow[currentFrame]);

    // currentFrame is NOT updated here — it advances in present() in lockstep with engine.endFrame
    return engine.beginFrame(fences, imageAvailableSemaphores[currentFrame]);
}

void RenderGraphExecutor::present(VkSmol& engine) {
    uint32_t presentGroupId = 0;
    for (const CompiledSubmissionGroup& group : graph.submissionGroups)
        if (group.signalsPresentSemaphore) { presentGroupId = group.handle.id; break; }

    engine.endFrame(groupSemaphores[presentGroupId][currentFrame]);
    currentFrame = (currentFrame + 1) % MAX_FRAME_IN_FLIGHT;
}

// ——— Recording ———————————————————————————————————————————————————————————————

CommandBuffer& RenderGraphExecutor::beginRecording(const SubmissionGroupHandle& group, VkSmol& engine) {
    (void)engine;
    CommandBuffer& cmd = groupCommandBuffers[group.id][currentFrame];
    cmd.reset();
    cmd.beginRecording();
    return cmd;
}

void RenderGraphExecutor::endRecording(const SubmissionGroupHandle& group, VkSmol& engine) {
    const CompiledSubmissionGroup& compiled = graph.submissionGroups[group.id];
    CommandBuffer& cmd = groupCommandBuffers[group.id][currentFrame];

    std::vector<const Semaphore*> waitSemaphores;
    std::vector<VkPipelineStageFlags> waitStages;

    if (compiled.waitsOnImageAvailable) {
        waitSemaphores.push_back(&imageAvailableSemaphores[currentFrame]);
        waitStages.push_back(compiled.imageAvailableWaitStage);
    }
    for (const SubmissionGroupHandle& dep : compiled.dependencies) {
        waitSemaphores.push_back(&groupSemaphores[dep.id][currentFrame]);
        waitStages.push_back(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    }

    engine.submit(
        cmd,
        groupFences[group.id][currentFrame],
        waitSemaphores, waitStages,
        { &groupSemaphores[group.id][currentFrame] }
    );
}

void RenderGraphExecutor::swapBindings(const ImageHandle& a, const ImageHandle& b) {
    std::swap(bindings[a.id],      bindings[b.id]);
    std::swap(ownedImages[a.id],   ownedImages[b.id]);
    std::swap(ownedViews[a.id],    ownedViews[b.id]);
    std::swap(ownedSamplers[a.id], ownedSamplers[b.id]);
}

Image& RenderGraphExecutor::getImage(const ImageHandle& handle) {
    return ownedImages.at(handle.id);
}

const ImageView& RenderGraphExecutor::getView(const ImageHandle& handle) const {
    return ownedViews.at(handle.id);
}

const Sampler& RenderGraphExecutor::getSampler(const ImageHandle& handle) const {
    return ownedSamplers.at(handle.id);
}

void RenderGraphExecutor::bindImage(const ImageHandle& handle, VkImage image, VkImageView view) {
    bindings[handle.id] = {
        .image = image,
        .view = view,
    };
}

void RenderGraphExecutor::bindBuffer(const BufferHandle& handle, VkBuffer buffer) {
    bufferBindings[handle.id] = buffer;
}

Buffer& RenderGraphExecutor::getBuffer(const BufferHandle& handle, uint32_t frameIndex) {
    if (graph.buffers[handle.id].perFrame) {
        uint32_t f = (frameIndex == UINT32_MAX) ? currentFrame : frameIndex;
        return ownedPerFrameBuffers.at(handle.id)[f];
    }
    return ownedBuffers.at(handle.id);
}

void RenderGraphExecutor::resizeBuffer(VkSmol& engine, const BufferHandle& handle, size_t newSize) {
    auto it = ownedBuffers.find(handle.id);
    if (it == ownedBuffers.end()) return;
    if (!graph.buffers[handle.id].initialData.empty()) return; // static buffers can't be resized

    engine.destroyBuffer(it->second);
    ownedBuffers.erase(it);

    graph.buffers[handle.id].size = newSize;

    VkBufferUsageFlags usageFlags = graph.buffers[handle.id].extraUsageFlags;
    for (const CompiledPass& pass : graph.compiledPasses)
        for (const BufferAccess& access : pass.bufferAccesses)
            if (access.buffer.id == handle.id)
                usageFlags |= bufferUsageTypeToVkFlags(access.info.usage);

    Buffer buf = engine.createMappedBuffer(usageFlags, newSize, graph.buffers[handle.id].name);
    bufferBindings[handle.id] = buf.get();
    ownedBuffers.emplace(handle.id, std::move(buf));
}

void RenderGraphExecutor::emitBarriers(const CommandBuffer& commandBuffer, const PassHandle& passHandle) const {
    for (const PassImageBarrier& barrier : graph.compiledPasses[passHandle.id].barriersBefore) {
        emitBarrier(commandBuffer, barrier);
    }

    const std::vector<PassBufferBarrier>& bufBarriers = graph.compiledPasses[passHandle.id].bufferBarriersBefore;

    for (const PassBufferBarrier& b : bufBarriers) {
        auto srcVk = bufferAccessToVk(b.srcAccess);
        auto dstVk = bufferAccessToVk(b.dstAccess);
        VkBuffer buf = graph.buffers[b.buffer.id].perFrame
            ? ownedPerFrameBuffers.at(b.buffer.id)[currentFrame].get()
            : bufferBindings.at(b.buffer.id);
        VkBufferMemoryBarrier barrier{
            .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask       = srcVk.access,
            .dstAccessMask       = dstVk.access,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer              = buf,
            .offset              = 0,
            .size                = VK_WHOLE_SIZE,
        };
        vkCmdPipelineBarrier(
            commandBuffer.get(),
            srcVk.stage, dstVk.stage,
            0,
            0, nullptr,
            1, &barrier,
            0, nullptr
        );
    }
}

void RenderGraphExecutor::emitOutputBarriers(const CommandBuffer& commandBuffer) const {
    for (const PassImageBarrier& barrier : graph.outputBarriers) {
        emitBarrier(commandBuffer, barrier);
    }
}

std::vector<AttachmentInfo> RenderGraphExecutor::getColorAttachment(const PassHandle& passHandle) const {
    std::vector<AttachmentInfo> attachments = {};

    for (const ImageAccess& access : graph.compiledPasses[passHandle.id].imageAccesses) {
        if (access.info.usage == ImageUsageType::ColorAttachment) {
            const ImageBinding& binding = bindings.at(access.image.id);
            VulkanImageState state = imageAccessToVk(access.info);
            attachments.push_back({
                .view   = binding.view,
                .layout = state.layout,
                .loadOp = attachmentLoadToVk(access.info.attachmentLoad),
            });
        }
    }
    if (attachments.size() == 0) throw std::runtime_error("No color attachment found in pass");

    return attachments;
}

const ImageBinding& RenderGraphExecutor::getBinding(const ImageHandle& handle) const {
    return bindings.at(handle.id);
}

ImageBinding RenderGraphExecutor::getTransferSrc(const PassHandle& passHandle) const {
    for (const ImageAccess& access : graph.compiledPasses[passHandle.id].imageAccesses) {
        if (access.info.usage == ImageUsageType::TransferSrc) {
            return bindings.at(access.image.id);
        }
    }
    throw std::runtime_error("No transfer src image found in pass");
}

ImageBinding RenderGraphExecutor::getTransferDst(const PassHandle& passHandle) const {
    for (const ImageAccess& access : graph.compiledPasses[passHandle.id].imageAccesses) {
        if (access.info.usage == ImageUsageType::TransferDst) {
            return bindings.at(access.image.id);
        }
    }
    throw std::runtime_error("No transfer dst image found in pass");
}

void RenderGraphExecutor::emitBarrier(const CommandBuffer& commandBuffer, const PassImageBarrier& barrier) const {
    ImageBinding binding = bindings.at(barrier.image.id);
    
    VulkanImageState src = imageAccessToVk(barrier.srcAccess);
    VulkanImageState dst = imageAccessToVk(barrier.dstAccess);
    
    ImageBarrier b;
    b.call(
        commandBuffer,
        binding.image,
        src.layout, dst.layout,
        src.access, dst.access,
        src.stage, dst.stage,
        dst.aspect
    );
}
