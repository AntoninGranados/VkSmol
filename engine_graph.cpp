#include "engine.hpp"

#include <stdexcept>
#include <utility>
#include <algorithm>

#include "./image/image_barrier.hpp"
#include "./utils.hpp"

#include "./graph/graph_utils.hpp"
#include "./graph/builder_resource.hpp"
#include "./graph/compiled_resource.hpp"
#include "./graph/render_graph_builder.hpp"

// ——— Graph setup —————————————————————————————————————————————————————————————

void VkSmol::setGraph(RenderGraphBuilder& builder) {
    graph = builder.compile();
}

// ——— Images ——————————————————————————————————————————————————————————————————

void VkSmol::initImages() {
    for (uint32_t id = 0; id < graph.images.size(); ++id) {
        const ImageInfo& info = graph.images[id];
        if (info.ownership == ImageOwnership::Imported) continue;

        VkImageUsageFlags usageFlags = info.extraUsageFlags;
        for (const CompiledPass& pass : graph.compiledPasses)
            for (const ImageAccess& access : pass.imageAccesses)
                if (access.image.id == id)
                    usageFlags |= imageUsageTypeToVkFlags(access.info.usage);

        Image img  = createImage(info.width, info.height, info.format, usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, info.name);
        ImageView view = createImageView(img, info.name + "View");
        Sampler   smp  = createSampler(info.name + "Sampler");

        ImageHandle handle = { .id = id };
        bindImage(handle, img.get(), view.get());
        ownedImages.emplace(id, std::move(img));
        ownedViews.emplace(id, std::move(view));
        ownedSamplers.emplace(id, std::move(smp));
    }
}

void VkSmol::destroyImages() {
    for (auto& [id, img] : ownedImages) {
        destroySampler(ownedSamplers.at(id));
        destroyImageView(ownedViews.at(id));
        destroyImage(img);
        bindings.erase(id);
    }
    ownedSamplers.clear();
    ownedViews.clear();
    ownedImages.clear();
}

// ——— Buffers —————————————————————————————————————————————————————————————————

void VkSmol::initBuffers() {
    for (uint32_t id = 0; id < graph.buffers.size(); ++id) {
        const BufferInfo& info = graph.buffers[id];
        if (info.ownership == BufferOwnership::Imported || info.perFrame) continue;

        VkBufferUsageFlags usageFlags = info.extraUsageFlags;
        for (const CompiledPass& pass : graph.compiledPasses)
            for (const BufferAccess& access : pass.bufferAccesses)
                if (access.buffer.id == id)
                    usageFlags |= bufferUsageTypeToVkFlags(access.info.usage);

        Buffer buf = info.initialData.empty()
            ? createMappedBuffer(usageFlags, info.size, info.name)
            : createBuffer(usageFlags, info.size, (void*)info.initialData.data(), info.name);
        bufferBindings[id] = buf.get();
        ownedBuffers.emplace(id, std::move(buf));
    }
}

void VkSmol::destroyBuffers() {
    for (auto& [id, buf] : ownedBuffers) {
        destroyBuffer(buf);
        bufferBindings.erase(id);
    }
    ownedBuffers.clear();
}

void VkSmol::initPerFrameBuffers() {
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
            arr[f] = createMappedBuffer(usageFlags, info.size, info.name + "[" + std::to_string(f) + "]");
    }
}

void VkSmol::destroyPerFrameBuffers() {
    for (auto& [id, arr] : ownedPerFrameBuffers)
        for (Buffer& buf : arr)
            destroyBuffer(buf);
    ownedPerFrameBuffers.clear();
}

// ——— Pipelines ———————————————————————————————————————————————————————————————

void VkSmol::initPipelines() {
    reloadPipelines();
}

void VkSmol::destroyPipelines() {
    for (auto& [id, pipeline] : ownedComputePipelines)
        destroyComputePipeline(pipeline);
    ownedComputePipelines.clear();
    for (auto& [id, pipeline] : ownedGraphicsPipelines)
        destroyGraphicsPipeline(pipeline);
    ownedGraphicsPipelines.clear();
}

// ——— Sync ————————————————————————————————————————————————————————————————————

void VkSmol::initSync() {
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
        imageAvailableSemaphores[f].init(device);
        for (uint32_t g = 0; g < numGroups; ++g) {
            groupSemaphores[g][f].init(device);
            groupFences[g][f].init(device, true);
            groupCommandBuffers[g][f] = allocCommandBuffer();
        }
    }
}

void VkSmol::destroySync() {
    for (uint32_t f = 0; f < MAX_FRAME_IN_FLIGHT; ++f) {
        imageAvailableSemaphores[f].destroy(device);
        for (uint32_t g = 0; g < groupSemaphores.size(); ++g) {
            groupSemaphores[g][f].destroy(device);
            groupFences[g][f].destroy(device);
            freeCommandBuffer(groupCommandBuffers[g][f]);
        }
    }
    imageAvailableSemaphores.clear();
    groupSemaphores.clear();
    groupFences.clear();
    groupCommandBuffers.clear();
}

// ——— Public lifecycle ————————————————————————————————————————————————————————

void VkSmol::initGraph() {
    initImages();
    initBuffers();
    initPerFrameBuffers();
    initPipelines();
    initSync();
}

void VkSmol::destroyGraph() {
    destroySync();
    destroyPipelines();
    destroyPerFrameBuffers();
    destroyBuffers();
    destroyImages();
}

void VkSmol::resizeGraph(uint32_t width, uint32_t height) {
    destroyImages();
    for (ImageInfo& info : graph.images) {
        if (info.ownership != ImageOwnership::Imported) {
            info.width  = width;
            info.height = height;
        }
    }
    initImages();
}

// ——— Frame ———————————————————————————————————————————————————————————————————

std::optional<FrameContext> VkSmol::beginFrame() {
    std::vector<const Fence*> fences;
    for (auto& groupFenceRow : groupFences)
        fences.push_back(&groupFenceRow[currentFrame]);

    Fence::wait(fences, device);
    resourceManager.collectFrame(currentFrame);
    resourceManager.setRetirementFrame(currentFrame);

    const uint32_t result = swapchain.getNextImageIndex(device, imageAvailableSemaphores[currentFrame]);
    if (result == RECREATE_SWAPCHAIN) {
        recreateSwapchain();
        return {};
    }

    Fence::reset(fences, device);
    imageIndex = result;

    return FrameContext{
        .extent           = getExtent(),
        .currentFrame     = currentFrame,
        .imageIndex       = imageIndex,
        .swapchainGeneration = swapchainGeneration,
    };
}

void VkSmol::present() {
    uint32_t presentGroupId = 0;
    for (const CompiledSubmissionGroup& group : graph.submissionGroups)
        if (group.signalsPresentSemaphore) { presentGroupId = group.handle.id; break; }

    const uint32_t result = presentQueue.present(
        { &swapchain }, { imageIndex }, { &groupSemaphores[presentGroupId][currentFrame] });
    if (result == RECREATE_SWAPCHAIN || framebufferResized) {
        recreateSwapchain();
    }

    currentFrame = (currentFrame + 1) % MAX_FRAME_IN_FLIGHT;
}

// ——— Recording ———————————————————————————————————————————————————————————————

CommandBuffer& VkSmol::beginRecording(const SubmissionGroupHandle& group) {
    CommandBuffer& cmd = groupCommandBuffers[group.id][currentFrame];
    cmd.reset();
    cmd.beginRecording();
    return cmd;
}

void VkSmol::endRecording(const SubmissionGroupHandle& group) {
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

    submit(cmd, groupFences[group.id][currentFrame], waitSemaphores, waitStages,
           { &groupSemaphores[group.id][currentFrame] });
}

// ——— Images (runtime) ————————————————————————————————————————————————————————

void VkSmol::swapBindings(const ImageHandle& a, const ImageHandle& b) {
    std::swap(bindings[a.id],      bindings[b.id]);
    std::swap(ownedImages[a.id],   ownedImages[b.id]);
    std::swap(ownedViews[a.id],    ownedViews[b.id]);
    std::swap(ownedSamplers[a.id], ownedSamplers[b.id]);
}

void VkSmol::bindImage(const ImageHandle& handle, VkImage image, VkImageView view) {
    bindings[handle.id] = { .image = image, .view = view };
}

const ImageBinding& VkSmol::getBinding(const ImageHandle& handle) const {
    return bindings.at(handle.id);
}

Image& VkSmol::getImage(const ImageHandle& handle) {
    return ownedImages.at(handle.id);
}

const ImageView& VkSmol::getView(const ImageHandle& handle) const {
    return ownedViews.at(handle.id);
}

const Sampler& VkSmol::getSampler(const ImageHandle& handle) const {
    return ownedSamplers.at(handle.id);
}

// ——— Buffers (runtime) ———————————————————————————————————————————————————————

void VkSmol::bindBuffer(const BufferHandle& handle, VkBuffer buffer) {
    bufferBindings[handle.id] = buffer;
}

Buffer& VkSmol::getBuffer(const BufferHandle& handle, uint32_t frameIndex) {
    if (graph.buffers[handle.id].perFrame) {
        uint32_t f = (frameIndex == UINT32_MAX) ? currentFrame : frameIndex;
        return ownedPerFrameBuffers.at(handle.id)[f];
    }
    return ownedBuffers.at(handle.id);
}

bool VkSmol::resizeBuffer(const BufferHandle& handle, size_t newSize) {
    if (graph.buffers[handle.id].size >= newSize) return false;
    if (!graph.buffers[handle.id].initialData.empty()) return false;

    graph.buffers[handle.id].size = newSize;

    VkBufferUsageFlags usageFlags = graph.buffers[handle.id].extraUsageFlags;
    for (const CompiledPass& pass : graph.compiledPasses)
        for (const BufferAccess& access : pass.bufferAccesses)
            if (access.buffer.id == handle.id)
                usageFlags |= bufferUsageTypeToVkFlags(access.info.usage);

    auto it = ownedBuffers.find(handle.id);
    if (it != ownedBuffers.end()) {
        destroyBuffer(it->second);
        ownedBuffers.erase(it);
        Buffer buf = createMappedBuffer(usageFlags, newSize, graph.buffers[handle.id].name);
        bufferBindings[handle.id] = buf.get();
        ownedBuffers.emplace(handle.id, std::move(buf));
        return true;
    }

    auto pfIt = ownedPerFrameBuffers.find(handle.id);
    if (pfIt != ownedPerFrameBuffers.end()) {
        for (Buffer& buf : pfIt->second) {
            destroyBuffer(buf);
            buf = createMappedBuffer(usageFlags, newSize, graph.buffers[handle.id].name);
        }
        return true;
    }

    return false;
}

// ——— Pipelines (runtime) —————————————————————————————————————————————————————

ComputePipeline& VkSmol::getComputePipeline(ComputePipelineHandle handle) {
    return ownedComputePipelines.at(handle.id);
}

GraphicsPipeline& VkSmol::getGraphicsPipeline(GraphicsPipelineHandle handle) {
    return ownedGraphicsPipelines.at(handle.id);
}

void VkSmol::reloadPipelines() {
    for (uint32_t id = 0; id < graph.computePipelines.size(); ++id) {
        const ComputePipelineInfo& info = graph.computePipelines[id];
        reloadComputePipeline(ownedComputePipelines[id], info.shaderPath, info.layouts);
    }
    for (uint32_t id = 0; id < graph.graphicsPipelines.size(); ++id) {
        const GraphicsPipelineInfo& info = graph.graphicsPipelines[id];
        VkPipelineVertexInputStateCreateInfo vi{
            .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount   = 1,
            .pVertexBindingDescriptions      = &info.bindingDescription,
            .vertexAttributeDescriptionCount = static_cast<uint32_t>(info.attributeDescriptions.size()),
            .pVertexAttributeDescriptions    = info.attributeDescriptions.data(),
        };
        reloadGraphicsPipeline(ownedGraphicsPipelines[id], vi, info.shaderPaths, info.layouts);
    }
}

// ——— Barriers ————————————————————————————————————————————————————————————————

void VkSmol::emitBarriers(const CommandBuffer& commandBuffer, const PassHandle& passHandle) const {
    for (const PassImageBarrier& barrier : graph.compiledPasses[passHandle.id].barriersBefore)
        emitBarrier(commandBuffer, barrier);

    for (const PassBufferBarrier& b : graph.compiledPasses[passHandle.id].bufferBarriersBefore) {
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
        vkCmdPipelineBarrier(commandBuffer.get(), srcVk.stage, dstVk.stage,
                             0, 0, nullptr, 1, &barrier, 0, nullptr);
    }
}

void VkSmol::emitOutputBarriers(const CommandBuffer& commandBuffer) const {
    for (const PassImageBarrier& barrier : graph.outputBarriers)
        emitBarrier(commandBuffer, barrier);
}

void VkSmol::emitBarrier(const CommandBuffer& commandBuffer, const PassImageBarrier& barrier) const {
    ImageBinding binding = bindings.at(barrier.image.id);
    VulkanImageState src = imageAccessToVk(barrier.srcAccess);
    VulkanImageState dst = imageAccessToVk(barrier.dstAccess);
    ImageBarrier b;
    b.call(commandBuffer, binding.image,
           src.layout, dst.layout,
           src.access, dst.access,
           src.stage, dst.stage,
           dst.aspect);
}

// ——— Attachment / Transfer queries ———————————————————————————————————————————

std::vector<AttachmentInfo> VkSmol::getColorAttachment(const PassHandle& passHandle) const {
    std::vector<AttachmentInfo> attachments;
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
    if (attachments.empty()) throw std::runtime_error("No color attachment found in pass");
    return attachments;
}

ImageBinding VkSmol::getTransferSrc(const PassHandle& passHandle) const {
    for (const ImageAccess& access : graph.compiledPasses[passHandle.id].imageAccesses)
        if (access.info.usage == ImageUsageType::TransferSrc)
            return bindings.at(access.image.id);
    throw std::runtime_error("No transfer src image found in pass");
}

ImageBinding VkSmol::getTransferDst(const PassHandle& passHandle) const {
    for (const ImageAccess& access : graph.compiledPasses[passHandle.id].imageAccesses)
        if (access.info.usage == ImageUsageType::TransferDst)
            return bindings.at(access.image.id);
    throw std::runtime_error("No transfer dst image found in pass");
}
