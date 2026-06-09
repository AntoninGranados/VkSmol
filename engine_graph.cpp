#include "engine.hpp"

#include <stdexcept>
#include <utility>
#include <algorithm>
#include <cassert>

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

// ——— Descriptor sets —————————————————————————————————————————————————————————

void VkSmol::initDescriptorSets() {
    for (const CompiledPass& pass : graph.compiledPasses) {
        if (pass.descriptorBindings.empty()) continue;
        uint32_t passId = pass.pass.id;

        auto decls = pass.descriptorBindings;
        std::sort(decls.begin(), decls.end(), [](const DescriptorBindingDecl& a, const DescriptorBindingDecl& b) {
            return a.binding < b.binding;
        });

        DescriptorSetLayout& layout = passDescriptorLayouts[passId];
        for (const DescriptorBindingDecl& decl : decls)
            layout.addBinding(decl.binding, decl.stageFlags, decl.descriptorType);
        initDescriptorSetLayout(layout);

        passDescriptorGroups[passId] = createDescriptorSetGroup(layout, pass.name + "Descriptors");
    }
}

void VkSmol::destroyDescriptorSets() {
    for (auto& [id, group] : passDescriptorGroups)
        destroyDescriptorSetGroup(group);
    passDescriptorGroups.clear();
    for (auto& [id, layout] : passDescriptorLayouts)
        destroyDescriptorSetLayout(layout);
    passDescriptorLayouts.clear();
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
    initDescriptorSets();
    initPipelines();
    initSync();
    for (const CompiledPass& pass : graph.compiledPasses)
        if (passDescriptorGroups.count(pass.pass.id))
            for (uint32_t f = 0; f < MAX_FRAME_IN_FLIGHT; ++f)
                updateDescriptors(pass.pass, f);
}

void VkSmol::destroyGraph() {
    destroySync();
    destroyPipelines();
    destroyDescriptorSets();
    destroyPerFrameBuffers();
    destroyBuffers();
    destroyImages();
}

void VkSmol::resizeImage(ImageHandle handle, uint32_t width, uint32_t height) {
    uint32_t id = handle.id;
    ImageInfo& info = graph.images[id];
    if (info.ownership == ImageOwnership::Imported) return;

    destroySampler(ownedSamplers.at(id));
    destroyImageView(ownedViews.at(id));
    destroyImage(ownedImages.at(id));
    ownedSamplers.erase(id);
    ownedViews.erase(id);
    ownedImages.erase(id);
    bindings.erase(id);

    info.width  = width;
    info.height = height;

    VkImageUsageFlags usageFlags = info.extraUsageFlags;
    for (const CompiledPass& pass : graph.compiledPasses)
        for (const ImageAccess& access : pass.imageAccesses)
            if (access.image.id == id)
                usageFlags |= imageUsageTypeToVkFlags(access.info.usage);

    Image     img  = createImage(width, height, info.format, usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, info.name);
    ImageView view = createImageView(img, info.name + "View");
    Sampler   smp  = createSampler(info.name + "Sampler");

    bindImage(handle, img.get(), view.get());
    ownedImages.emplace(id, std::move(img));
    ownedViews.emplace(id, std::move(view));
    ownedSamplers.emplace(id, std::move(smp));

    for (uint32_t f = 0; f < MAX_FRAME_IN_FLIGHT; ++f)
        refreshDescriptorsForImage(id, f);
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
    for (const CompiledPass& pass : graph.compiledPasses) {
        if (!passDescriptorGroups.count(pass.pass.id)) continue;
        for (const DescriptorBindingDecl& decl : pass.descriptorBindings) {
            if (decl.isImage && (decl.handleId == a.id || decl.handleId == b.id)) {
                updateDescriptors(pass.pass, currentFrame);
                break;
            }
        }
    }
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
        refreshDescriptorsForBuffer(handle.id);
        return true;
    }

    auto pfIt = ownedPerFrameBuffers.find(handle.id);
    if (pfIt != ownedPerFrameBuffers.end()) {
        for (Buffer& buf : pfIt->second) {
            destroyBuffer(buf);
            buf = createMappedBuffer(usageFlags, newSize, graph.buffers[handle.id].name);
        }
        refreshDescriptorsForBuffer(handle.id);
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
        std::vector<const DescriptorSetLayout*> layouts;
        if (info.passId != InvalidHandle) {
            auto it = passDescriptorLayouts.find(info.passId);
            if (it != passDescriptorLayouts.end())
                layouts = { &it->second };
        }
        reloadComputePipeline(ownedComputePipelines[id], info.shaderPath, layouts);
    }
    for (uint32_t id = 0; id < graph.graphicsPipelines.size(); ++id) {
        const GraphicsPipelineInfo& info = graph.graphicsPipelines[id];
        std::vector<const DescriptorSetLayout*> layouts;
        if (info.passId != InvalidHandle) {
            auto it = passDescriptorLayouts.find(info.passId);
            if (it != passDescriptorLayouts.end())
                layouts = { &it->second };
        }
        VkPipelineVertexInputStateCreateInfo vi{
            .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount   = 1,
            .pVertexBindingDescriptions      = &info.bindingDescription,
            .vertexAttributeDescriptionCount = static_cast<uint32_t>(info.attributeDescriptions.size()),
            .pVertexAttributeDescriptions    = info.attributeDescriptions.data(),
        };
        reloadGraphicsPipeline(ownedGraphicsPipelines[id], vi, info.shaderPaths, layouts);
    }
}

// ——— Descriptor management ———————————————————————————————————————————————————

void VkSmol::refreshDescriptorsForImage(uint32_t imageId, uint32_t frameIndex) {
    for (const CompiledPass& pass : graph.compiledPasses) {
        if (!passDescriptorGroups.count(pass.pass.id)) continue;
        for (const DescriptorBindingDecl& decl : pass.descriptorBindings) {
            if (decl.isImage && decl.handleId == imageId) {
                updateDescriptors(pass.pass, frameIndex);
                break;
            }
        }
    }
}

void VkSmol::refreshDescriptorsForBuffer(uint32_t bufferId) {
    for (const CompiledPass& pass : graph.compiledPasses) {
        if (!passDescriptorGroups.count(pass.pass.id)) continue;
        for (const DescriptorBindingDecl& decl : pass.descriptorBindings) {
            if (!decl.isImage && decl.handleId == bufferId) {
                for (uint32_t f = 0; f < MAX_FRAME_IN_FLIGHT; ++f)
                    updateDescriptors(pass.pass, f);
                break;
            }
        }
    }
}

void VkSmol::updateDescriptors(const PassHandle& pass, uint32_t frameIndex) {
    uint32_t passId = pass.id;
    auto groupIt = passDescriptorGroups.find(passId);
    if (groupIt == passDescriptorGroups.end()) return;

    const CompiledPass& compiled = graph.compiledPasses[passId];
    DescriptorWriter writer(passDescriptorLayouts.at(passId));
    for (const DescriptorBindingDecl& decl : compiled.descriptorBindings) {
        if (decl.isImage) {
            ImageHandle h = { .id = decl.handleId };
            if (decl.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                writer.combinedImageSampler(decl.binding, getView(h), getSampler(h));
            else if (decl.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                writer.storageImage(decl.binding, getView(h));
        } else {
            BufferHandle h = { .id = decl.handleId };
            if (decl.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                writer.uniform(decl.binding, getBuffer(h, frameIndex));
            else if (decl.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
                writer.storage(decl.binding, getBuffer(h, frameIndex));
        }
    }
    updateDescriptorSetGroup(groupIt->second, frameIndex, writer);
}

void VkSmol::bindDescriptors(const CommandBuffer& cmd, const PassHandle& passHandle) {
    uint32_t passId = passHandle.id;
    auto groupIt = passDescriptorGroups.find(passId);
    if (groupIt == passDescriptorGroups.end()) return;

    const CompiledPass& compiled = graph.compiledPasses[passId];
    assert(compiled.pipelineId.has_value());

    VkPipelineLayout layout;
    VkPipelineBindPoint bindPoint;
    if (compiled.type == PassType::Compute) {
        layout     = ownedComputePipelines.at(*compiled.pipelineId).getLayout();
        bindPoint  = VK_PIPELINE_BIND_POINT_COMPUTE;
    } else {
        layout     = ownedGraphicsPipelines.at(*compiled.pipelineId).getLayout();
        bindPoint  = VK_PIPELINE_BIND_POINT_GRAPHICS;
    }

    groupIt->second.at(currentFrame).bind(cmd, layout, bindPoint);
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
