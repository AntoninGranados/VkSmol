#include "engine.hpp"

#include <stdexcept>
#include <utility>
#include <algorithm>
#include <cassert>
#include <format>
#include <unordered_set>

#include "./image/image_barrier.hpp"
#include "./utils.hpp"

#include "./graph/graph_utils.hpp"
#include "./graph/builder_resource.hpp"
#include "./graph/compiled_resource.hpp"
#include "./graph/render_graph_builder.hpp"

namespace VkSmol {

// ——— Graph setup —————————————————————————————————————————————————————————————

void Engine::setGraph(RenderGraphBuilder& builder) {
    graph = builder.compile();
}

// ——— Images ——————————————————————————————————————————————————————————————————

void Engine::initImages() {
    for (uint32_t id = 0; id < graph.images.size(); ++id) {
        const ImageInfo& info = graph.images[id];
        if (info.ownership == ImageOwnership::Imported || ownedImages.contains(id)) continue;

        Image img  = createImage(info.width, info.height, info.format, imageUsageFlags(graph, id), VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, info.name);
        ImageView view = createImageView(img, info.name + "View");
        Sampler   smp  = createSampler(info.name + "Sampler");

        ImageHandle handle = { .id = id };
        bindImage(handle, img.get(), view.get());
        ownedImages.emplace(id, std::move(img));
        ownedViews.emplace(id, std::move(view));
        ownedSamplers.emplace(id, std::move(smp));
    }
}

void Engine::destroyImages() {
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

VkImageUsageFlags Engine::imageUsageFlags(const CompiledRenderGraph& compiled, uint32_t imageId) const {
    VkImageUsageFlags usageFlags = compiled.images[imageId].extraUsageFlags;
    for (const CompiledPass& pass : compiled.compiledPasses)
        for (const ImageAccess& access : pass.imageAccesses)
            if (access.image.id == imageId)
                usageFlags |= imageUsageTypeToVkFlags(access.info.usage);
    return usageFlags;
}

// ——— Buffers —————————————————————————————————————————————————————————————————

void Engine::initBuffers() {
    for (uint32_t id = 0; id < graph.buffers.size(); ++id) {
        const BufferInfo& info = graph.buffers[id];
        if (info.ownership == BufferOwnership::Imported || ownedBuffers.contains(id)) continue;

        void* initialData = info.initialData.empty() ? nullptr : (void*)info.initialData.data();
        Buffer buf = createBuffer(bufferUsageFlags(graph, id), info.size, 0, initialData, info.name);
        bufferBindings[id] = buf.get();
        ownedBuffers.emplace(id, std::move(buf));
    }
}

void Engine::destroyBuffers() {
    for (auto& [id, buf] : ownedBuffers) {
        destroyBuffer(buf);
        bufferBindings.erase(id);
    }
    ownedBuffers.clear();
}

VkBufferUsageFlags Engine::bufferUsageFlags(const CompiledRenderGraph& compiled, uint32_t bufferId) const {
    VkBufferUsageFlags usageFlags = compiled.buffers[bufferId].extraUsageFlags
        | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    for (const CompiledPass& pass : compiled.compiledPasses)
        for (const BufferAccess& access : pass.bufferAccesses)
            if (access.buffer.id == bufferId)
                usageFlags |= bufferUsageTypeToVkFlags(access.info.usage);
    return usageFlags;
}

void Engine::replaceBuffer(uint32_t bufferId, size_t newSize, bool preserve) {
    BufferInfo& info = graph.buffers[bufferId];
    Buffer next = createBuffer(bufferUsageFlags(graph, bufferId), newSize, 0, nullptr, info.name);

    auto previous = ownedBuffers.find(bufferId);
    pendingBufferTransfers.push_back(PendingBufferReplace{
        .previous      = std::move(previous->second),
        .next          = next.get(),
        .preservedSize = preserve ? info.size : 0,
    });
    ownedBuffers.erase(previous);

    bufferBindings[bufferId] = next.get();
    ownedBuffers.emplace(bufferId, std::move(next));
    info.size = newSize;
    refreshDescriptorsForBuffer(bufferId);
}

size_t Engine::stage(const void* data, size_t size) {
    StagingArena& arena = stagingArenas[currentFrame];
    size_t offset = (arena.used + 15) & ~size_t(15);
    if (offset + size > arena.buffer.getSize()) {
        const size_t capacity = std::max(arena.buffer.getSize() * 2, size);
        destroyBuffer(arena.buffer);
        arena.buffer = createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, capacity, BufferAllocMapped, nullptr, "StagingBuffer");
        offset = 0;
    }
    arena.buffer.write(device, data, size, offset);
    arena.used = offset + size;
    return offset;
}

void Engine::flushBufferTransfers(const CommandBuffer& commandBuffer) {
    if (pendingBufferTransfers.empty()) return;

    constexpr VkAccessFlags transferAccess = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    emitMemoryBarrier(commandBuffer,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, transferAccess);

    struct WrittenRange { VkBuffer buffer; size_t begin, end; };
    std::vector<WrittenRange> written;
    auto serializeTransfers = [&] {
        emitMemoryBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, transferAccess);
        written.clear();
    };

    for (auto& transfer : pendingBufferTransfers) {
        if (auto* write = std::get_if<PendingBufferWrite>(&transfer)) {
            const size_t end = write->offset + write->data.size();
            for (const WrittenRange& range : written)
                if (range.buffer == write->dst && write->offset < range.end && range.begin < end) {
                    serializeTransfers();
                    break;
                }

            const size_t stagingOffset = stage(write->data.data(), write->data.size());
            const VkBufferCopy region{ .srcOffset = stagingOffset, .dstOffset = write->offset, .size = write->data.size() };
            vkCmdCopyBuffer(commandBuffer.get(), stagingArenas[currentFrame].buffer.get(), write->dst, 1, &region);
            written.push_back({ write->dst, write->offset, end });
        } else {
            auto& replace = std::get<PendingBufferReplace>(transfer);
            if (replace.preservedSize > 0) {
                serializeTransfers();
                const VkBufferCopy region{ .srcOffset = 0, .dstOffset = 0, .size = replace.preservedSize };
                vkCmdCopyBuffer(commandBuffer.get(), replace.previous.get(), replace.next, 1, &region);
                serializeTransfers();
            }
            destroyBuffer(replace.previous);
        }
    }
    pendingBufferTransfers.clear();

    emitMemoryBarrier(commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
}

void Engine::discardBufferTransfers() {
    for (auto& transfer : pendingBufferTransfers)
        if (auto* replace = std::get_if<PendingBufferReplace>(&transfer))
            destroyBuffer(replace->previous);
    pendingBufferTransfers.clear();
}

void Engine::emitMemoryBarrier(
    const CommandBuffer& commandBuffer,
    VkPipelineStageFlags srcStage, VkAccessFlags srcAccess,
    VkPipelineStageFlags dstStage, VkAccessFlags dstAccess
) const {
    const VkMemoryBarrier barrier{
        .sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = srcAccess,
        .dstAccessMask = dstAccess,
    };
    vkCmdPipelineBarrier(commandBuffer.get(), srcStage, dstStage, 0, 1, &barrier, 0, nullptr, 0, nullptr);
}

// ——— Descriptor sets —————————————————————————————————————————————————————————

void Engine::initDescriptorSets() {
    for (const CompiledPass& pass : graph.compiledPasses) {
        if (pass.descriptorBindings.empty()) continue;
        uint32_t passId = pass.pass.id;

        auto decls = pass.descriptorBindings;
        std::sort(decls.begin(), decls.end(), [](const DescriptorBindingDecl& a, const DescriptorBindingDecl& b) {
            return a.binding < b.binding;
        });

        uint32_t setCount = 0;
        for (const DescriptorBindingDecl& decl : decls)
            setCount = std::max(setCount, decl.set + 1);

        std::vector<DescriptorSetLayout>& layouts = passDescriptorLayouts[passId];
        layouts = std::vector<DescriptorSetLayout>(setCount);
        for (const DescriptorBindingDecl& decl : decls)
            layouts[decl.set].addBinding(decl.binding, decl.stageFlags, decl.descriptorType);

        std::vector<DescriptorSetGroup>& groups = passDescriptorGroups[passId];
        groups = std::vector<DescriptorSetGroup>(setCount);
        for (uint32_t set = 0; set < setCount; ++set) {
            initDescriptorSetLayout(layouts[set]);
            if (!layouts[set].getBindings().empty())
                groups[set] = createDescriptorSetGroup(layouts[set], std::format("{}Descriptors{}", pass.name, set));
        }
    }
}

void Engine::destroyDescriptorSets() {
    for (auto& [id, groups] : passDescriptorGroups)
        for (DescriptorSetGroup& group : groups)
            if (group.isValid()) destroyDescriptorSetGroup(group);
    passDescriptorGroups.clear();
    for (auto& [id, layouts] : passDescriptorLayouts)
        for (DescriptorSetLayout& layout : layouts)
            destroyDescriptorSetLayout(layout);
    passDescriptorLayouts.clear();
}

// ——— Pipelines ———————————————————————————————————————————————————————————————

std::vector<std::string> Engine::initPipelines() {
    std::vector<std::string> errors;
    auto layoutsOf = [&](uint32_t passId) -> std::vector<const DescriptorSetLayout*> {
        auto it = passDescriptorLayouts.find(passId);
        if (it == passDescriptorLayouts.end()) return {};
        std::vector<const DescriptorSetLayout*> layouts;
        for (const DescriptorSetLayout& layout : it->second) layouts.push_back(&layout);
        return layouts;
    };

    for (uint32_t id = 0; id < graph.computePipelines.size(); ++id) {
        const ComputePipelineInfo& info = graph.computePipelines[id];
        Shader shader;
        shader.init(device, VK_SHADER_STAGE_COMPUTE_BIT, compilePassShader(graph.compiledPasses[info.passId], VK_SHADER_STAGE_COMPUTE_BIT, info.shaderPath, info.shaderDefines, errors));
        ownedComputePipelines[id] = initComputePipeline(shader, layoutsOf(info.passId));
        shader.destroy(device);
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

        std::vector<Shader> shaders(info.shaderPaths.size());
        std::vector<const Shader*> shaderPointers;
        for (size_t i = 0; i < info.shaderPaths.size(); ++i) {
            const auto& [stage, path] = info.shaderPaths[i];
            shaders[i].init(device, stage, compilePassShader(graph.compiledPasses[info.passId], stage, path, info.shaderDefines, errors));
            shaderPointers.push_back(&shaders[i]);
        }
        ownedGraphicsPipelines[id] = initGraphicsPipeline(vi, shaderPointers, layoutsOf(info.passId));
        for (Shader& shader : shaders) shader.destroy(device);
    }

    return errors;
}

void Engine::destroyPipelines() {
    for (auto& [id, pipeline] : ownedComputePipelines)
        destroyComputePipeline(pipeline);
    ownedComputePipelines.clear();
    for (auto& [id, pipeline] : ownedGraphicsPipelines)
        destroyGraphicsPipeline(pipeline);
    ownedGraphicsPipelines.clear();
}

// ——— Sync ————————————————————————————————————————————————————————————————————

void Engine::initSync() {
    const uint32_t numGroups = static_cast<uint32_t>(graph.submissionGroups.size());

    if (!platform->isHeadless()) std::get<WindowedState>(displayState).imageAvailableSemaphores.resize(MAX_FRAME_IN_FLIGHT);
    groupSemaphores.resize(numGroups);
    groupFences.resize(numGroups);
    groupCommandBuffers.resize(numGroups);
    for (uint32_t g = 0; g < numGroups; ++g) {
        groupSemaphores[g].resize(MAX_FRAME_IN_FLIGHT);
        groupFences[g].resize(MAX_FRAME_IN_FLIGHT);
        groupCommandBuffers[g].resize(MAX_FRAME_IN_FLIGHT);
    }

    for (uint32_t f = 0; f < MAX_FRAME_IN_FLIGHT; ++f) {
        if (!platform->isHeadless()) std::get<WindowedState>(displayState).imageAvailableSemaphores[f].init(device);
        for (uint32_t g = 0; g < numGroups; ++g) {
            groupSemaphores[g][f].init(device);
            groupFences[g][f].init(device, true);
            groupCommandBuffers[g][f] = allocCommandBuffer();
        }
    }
}

void Engine::destroySync() {
    for (uint32_t f = 0; f < MAX_FRAME_IN_FLIGHT; ++f) {
        if (!platform->isHeadless()) std::get<WindowedState>(displayState).imageAvailableSemaphores[f].destroy(device);
        for (uint32_t g = 0; g < groupSemaphores.size(); ++g) {
            groupSemaphores[g][f].destroy(device);
            groupFences[g][f].destroy(device);
            freeCommandBuffer(groupCommandBuffers[g][f]);
        }
    }
    if (!platform->isHeadless()) std::get<WindowedState>(displayState).imageAvailableSemaphores.clear();
    groupSemaphores.clear();
    groupFences.clear();
    groupCommandBuffers.clear();
}

// ——— Public lifecycle ————————————————————————————————————————————————————————

std::vector<std::string> Engine::initGraph(std::string pipelineCachePath) {
    assert(ownedImages.empty() && "Engine::initGraph() called without a prior destroyGraph() — bindings would be left dangling");
    return initGraphState(std::move(pipelineCachePath));
}

std::vector<std::string> Engine::rebuildGraph(RenderGraphBuilder& builder, std::string pipelineCachePath) {
    CompiledRenderGraph next = builder.compile();
    if (!graphInitialized) {
        graph = std::move(next);
        return initGraphState(std::move(pipelineCachePath));
    }

    waitIdle();
    if (!pendingBufferTransfers.empty()) {
        CommandBuffer commandBuffer = CommandBuffer::beginSingleTimeCommands(device, immediateCommandPool);
        flushBufferTransfers(commandBuffer);
        CommandBuffer::endSingleTimeCommands(device, immediateCommandPool, graphicsQueue, commandBuffer);
    }

    ReusedResources reused = takeReusableResources(next);
    destroyGraph();
    graph = std::move(next);

    for (auto& [id, image] : reused.images) {
        ImageHandle handle = { .id = id };
        bindImage(handle, image.get(), reused.views.at(id).get());
        ownedImages.emplace(id, std::move(image));
        ownedViews.emplace(id, std::move(reused.views.at(id)));
        ownedSamplers.emplace(id, std::move(reused.samplers.at(id)));
    }
    for (auto& [id, buffer] : reused.buffers) {
        bufferBindings[id] = buffer.get();
        ownedBuffers.emplace(id, std::move(buffer));
    }

    return initGraphState(std::move(pipelineCachePath));
}

Engine::ReusedResources Engine::takeReusableResources(CompiledRenderGraph& next) {
    auto sameAccess = [](const std::optional<ImageAccessInfo>& a, const std::optional<ImageAccessInfo>& b) {
        if (a.has_value() != b.has_value()) return false;
        return !a.has_value() || (a->usage == b->usage && a->access == b->access);
    };

    ReusedResources reused;

    for (uint32_t nextId = 0; nextId < next.images.size(); ++nextId) {
        const ImageInfo& wanted = next.images[nextId];
        if (wanted.ownership == ImageOwnership::Imported) continue;

        for (auto& [id, image] : ownedImages) {
            const ImageInfo& current = graph.images[id];
            if (current.name != wanted.name || current.format != wanted.format || current.width != wanted.width ||
                current.height != wanted.height || current.depth != wanted.depth ||
                current.ownership != wanted.ownership || !sameAccess(current.initialAccess, wanted.initialAccess) ||
                !sameAccess(current.finalAccess, wanted.finalAccess) ||
                imageUsageFlags(graph, id) != imageUsageFlags(next, nextId))
                continue;

            reused.images.emplace(nextId, std::move(image));
            reused.views.emplace(nextId, std::move(ownedViews.at(id)));
            reused.samplers.emplace(nextId, std::move(ownedSamplers.at(id)));
            ownedImages.erase(id);
            ownedViews.erase(id);
            ownedSamplers.erase(id);
            bindings.erase(id);
            break;
        }
    }

    for (uint32_t nextId = 0; nextId < next.buffers.size(); ++nextId) {
        BufferInfo& wanted = next.buffers[nextId];
        if (wanted.ownership == BufferOwnership::Imported || !wanted.initialData.empty()) continue;

        for (auto& [id, buffer] : ownedBuffers) {
            const BufferInfo& current = graph.buffers[id];
            if (current.name != wanted.name || current.size < wanted.size || !current.initialData.empty() ||
                bufferUsageFlags(graph, id) != bufferUsageFlags(next, nextId))
                continue;

            wanted.size = current.size;
            reused.buffers.emplace(nextId, std::move(buffer));
            ownedBuffers.erase(id);
            bufferBindings.erase(id);
            break;
        }
    }

    return reused;
}

std::vector<std::string> Engine::initGraphState(std::string pipelineCachePath) {
    dirtyDescriptors.clear();
    pipelineCache.init(device, pipelineCachePath);
    initImages();
    initBuffers();
    initDescriptorSets();
    std::vector<std::string> errors = initPipelines();
    for (auto& [key, compiled] : compiledShaders)
        compiled.checked = false;
    initSync();
    for (const CompiledPass& pass : graph.compiledPasses)
        if (passDescriptorGroups.count(pass.pass.id))
            for (uint32_t f = 0; f < MAX_FRAME_IN_FLIGHT; ++f)
                updateDescriptors(pass.pass, f);

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(physicalDevice.get(), &props);
    timestampPeriod = props.limits.timestampPeriod;

    numTimestamps = 0;
    for (const CompiledPass& p : graph.compiledPasses)
        if (p.timestampIndex.has_value())
            numTimestamps = std::max(numTimestamps, *p.timestampIndex + 1);

    if (numTimestamps > 0) {
        VkQueryPoolCreateInfo info{};
        info.sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        info.queryType  = VK_QUERY_TYPE_TIMESTAMP;
        info.queryCount = numTimestamps * 2 * MAX_FRAME_IN_FLIGHT;
        vkCheck(vkCreateQueryPool(device.get(), &info, nullptr, &timestampPool), "Failed to create timestamp query pool");
    }
    graphInitialized = true;
    return errors;
}

void Engine::destroyGraph() {
    graphInitialized = false;
    if (timestampPool != VK_NULL_HANDLE) {
        vkDestroyQueryPool(device.get(), timestampPool, nullptr);
        timestampPool = VK_NULL_HANDLE;
    }
    numTimestamps = 0;
    dirtyDescriptors.clear();
    destroySync();
    destroyPipelines();
    pipelineCache.destroy(device);
    destroyDescriptorSets();
    discardBufferTransfers();
    destroyBuffers();
    destroyImages();
}

void Engine::resizeImage(ImageHandle handle, uint32_t width, uint32_t height) {
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

std::optional<FrameContext> Engine::beginFrame() {
    std::vector<const Fence*> fences;
    for (auto& groupFenceRow : groupFences)
        fences.push_back(&groupFenceRow[currentFrame]);

    Fence::wait(fences, device);

    if (timestampPool && numTimestamps > 0) {
        cachedTimestamps.resize(numTimestamps, -1.0);
        for (uint32_t i = 0; i < numTimestamps; ++i) {
            uint32_t base = (currentFrame * numTimestamps + i) * 2;
            uint64_t data[4] = {};
            vkGetQueryPoolResults(
                device.get(), timestampPool, base, 2,
                sizeof(data), data, 2 * sizeof(uint64_t),
                VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT
            );
            cachedTimestamps[i] = (data[1] && data[3])
                ? static_cast<double>(data[2] - data[0]) * timestampPeriod / 1e6
                : -1.0;
        }
    }

    for (auto& payload : retirementQueue[currentFrame]) payload(device);
    retirementQueue[currentFrame].clear();
    currentRetirementFrame = currentFrame;
    stagingArenas[currentFrame].used = 0;

    if (!platform->isHeadless()) {
        const std::optional<uint32_t> result = std::get<WindowedState>(displayState).swapchain.getNextImageIndex(
            device,
            std::get<WindowedState>(displayState).imageAvailableSemaphores[currentFrame]);
        if (!result) {
            recreateSwapchain();
            return {};
        }

        Fence::reset(fences, device);
        imageIndex = *result;

        return FrameContext{
            .extent              = getExtent(),
            .currentFrame        = currentFrame,
            .imageIndex          = imageIndex,
            .swapchainGeneration = std::get<WindowedState>(displayState).swapchainGeneration,
        };
    } else {
        Fence::reset(fences, device);
        return FrameContext{
            .extent       = getExtent(),
            .currentFrame = currentFrame,
        };
    }
}

void Engine::present() {
    assert(!platform->isHeadless());

    uint32_t presentGroupId = 0;
    for (const CompiledSubmissionGroup& group : graph.submissionGroups)
        if (group.signalsPresentSemaphore) { presentGroupId = group.handle.id; break; }

    const bool outOfDate = std::get<WindowedState>(displayState).presentQueue.present(
        { &std::get<WindowedState>(displayState).swapchain }, { imageIndex }, { &groupSemaphores[presentGroupId][currentFrame] });
    if (outOfDate || framebufferResized) {
        recreateSwapchain();
    }
}

void Engine::advanceFrame() {
    currentFrame = (currentFrame + 1) % MAX_FRAME_IN_FLIGHT;
}

// ——— Recording ———————————————————————————————————————————————————————————————

CommandBuffer& Engine::beginRecording(const SubmissionGroupHandle& group) {
    CommandBuffer& cmd = groupCommandBuffers[group.id][currentFrame];
    cmd.reset();
    cmd.beginRecording();
    flushBufferTransfers(cmd);
    return cmd;
}

void Engine::endRecording(const SubmissionGroupHandle& group) {
    const CompiledSubmissionGroup& compiled = graph.submissionGroups[group.id];
    CommandBuffer& cmd = groupCommandBuffers[group.id][currentFrame];

    std::vector<const Semaphore*> waitSemaphores;
    std::vector<VkPipelineStageFlags> waitStages;

    if (!platform->isHeadless() && compiled.waitsOnImageAvailable) {
        waitSemaphores.push_back(&std::get<WindowedState>(displayState).imageAvailableSemaphores[currentFrame]);
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

void Engine::swapBindings(const ImageHandle& a, const ImageHandle& b) {
    std::swap(bindings[a.id],      bindings[b.id]);
    std::swap(ownedImages[a.id],   ownedImages[b.id]);
    std::swap(ownedViews[a.id],    ownedViews[b.id]);
    std::swap(ownedSamplers[a.id], ownedSamplers[b.id]);
    for (const CompiledPass& pass : graph.compiledPasses) {
        if (!passDescriptorGroups.count(pass.pass.id)) continue;
        for (const DescriptorBindingDecl& decl : pass.descriptorBindings) {
            if (decl.isImage && (decl.handleId == a.id || decl.handleId == b.id)) {
                for (uint32_t f = 0; f < MAX_FRAME_IN_FLIGHT; ++f)
                    dirtyDescriptors.insert(pass.pass.id * MAX_FRAME_IN_FLIGHT + f);
                break;
            }
        }
    }
}

void Engine::bindImage(const ImageHandle& handle, VkImage image, VkImageView view) {
    bindings[handle.id] = { .image = image, .view = view };
}

const ImageBinding& Engine::getBinding(const ImageHandle& handle) const {
    return bindings.at(handle.id);
}

Image& Engine::getImage(const ImageHandle& handle) {
    return ownedImages.at(handle.id);
}

const ImageView& Engine::getView(const ImageHandle& handle) const {
    return ownedViews.at(handle.id);
}

const Sampler& Engine::getSampler(const ImageHandle& handle) const {
    return ownedSamplers.at(handle.id);
}

// ——— Buffers (runtime) ———————————————————————————————————————————————————————

void Engine::bindBuffer(const BufferHandle& handle, VkBuffer buffer) {
    bufferBindings[handle.id] = buffer;
}

Buffer& Engine::getBuffer(const BufferHandle& handle) {
    return ownedBuffers.at(handle.id);
}

bool Engine::resizeBuffer(const BufferHandle& handle, size_t newSize) {
    const BufferInfo& info = graph.buffers[handle.id];
    if (info.ownership == BufferOwnership::Imported || info.size >= newSize) return false;
    replaceBuffer(handle.id, newSize, false);
    return true;
}

void Engine::writeBuffer(const BufferHandle& handle, const void* data, size_t size, size_t offset) {
    if (size == 0) return;
    const BufferInfo& info = graph.buffers[handle.id];
    if (info.ownership == BufferOwnership::Imported)
        throw std::runtime_error("Cannot write to imported buffer \"" + info.name + "\"");
    if (offset + size > info.size)
        replaceBuffer(handle.id, std::max(offset + size, info.size * 2), true);

    const std::byte* bytes = static_cast<const std::byte*>(data);
    pendingBufferTransfers.push_back(PendingBufferWrite{
        .dst    = bufferBindings.at(handle.id),
        .offset = offset,
        .data   = std::vector<std::byte>(bytes, bytes + size),
    });
}

void Engine::readBuffer(const BufferHandle& handle, void* out, size_t size, size_t offset) {
    if (size == 0) return;
    Buffer readback = createBuffer(VK_BUFFER_USAGE_TRANSFER_DST_BIT, size, BufferAllocMapped, nullptr,
                                   graph.buffers[handle.id].name + "Readback");

    CommandBuffer commandBuffer = CommandBuffer::beginSingleTimeCommands(device, immediateCommandPool);
    flushBufferTransfers(commandBuffer);
    emitMemoryBarrier(commandBuffer,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    const VkBufferCopy region{ .srcOffset = offset, .dstOffset = 0, .size = size };
    vkCmdCopyBuffer(commandBuffer.get(), bufferBindings.at(handle.id), readback.get(), 1, &region);
    emitMemoryBarrier(commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_READ_BIT);
    CommandBuffer::endSingleTimeCommands(device, immediateCommandPool, graphicsQueue, commandBuffer);

    readback.read(device, out, size);
    readback.free(device);
    readback.destroy(device);
}

// ——— Pipelines (runtime) —————————————————————————————————————————————————————

ComputePipeline& Engine::getComputePipeline(ComputePipelineHandle handle) {
    return ownedComputePipelines.at(handle.id);
}

GraphicsPipeline& Engine::getGraphicsPipeline(GraphicsPipelineHandle handle) {
    return ownedGraphicsPipelines.at(handle.id);
}

void Engine::dispatch(const CommandBuffer& cmd, const PassHandle& passHandle, uint32_t x, uint32_t y) {
    flushBufferTransfers(cmd);
    const CompiledPass& compiled = graph.compiledPasses[passHandle.id];
    if (compiled.timestampIndex.has_value()) {
        uint32_t base = (currentFrame * numTimestamps + *compiled.timestampIndex) * 2;
        vkCmdResetQueryPool(cmd.get(), timestampPool, base, 2);
        vkCmdWriteTimestamp(cmd.get(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, timestampPool, base);
    }
    emitBarriers(cmd, passHandle);
    bindDescriptors(cmd, passHandle);
    ComputePipeline& pipeline = ownedComputePipelines.at(*compiled.pipelineId);
    pipeline.bind(cmd);
    pipeline.dispatch(cmd, x, y);
    if (compiled.timestampIndex.has_value()) {
        uint32_t base = (currentFrame * numTimestamps + *compiled.timestampIndex) * 2;
        vkCmdWriteTimestamp(cmd.get(), VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestampPool, base + 1);
    }
}

void Engine::beginGraphics(const CommandBuffer& cmd, const PassHandle& passHandle, const float clearColor[4]) {
    flushBufferTransfers(cmd);
    const CompiledPass& compiled = graph.compiledPasses[passHandle.id];
    if (compiled.timestampIndex.has_value()) {
        uint32_t base = (currentFrame * numTimestamps + *compiled.timestampIndex) * 2;
        vkCmdResetQueryPool(cmd.get(), timestampPool, base, 2);
        vkCmdWriteTimestamp(cmd.get(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, timestampPool, base);
    }
    emitBarriers(cmd, passHandle);
    std::vector<AttachmentInfo> attachments = getColorAttachment(passHandle);
    assert(attachments.size() == 1);
    VkClearColorValue vkClear = {};
    if (clearColor) memcpy(vkClear.float32, clearColor, 4 * sizeof(float));
    beginDynamicRenderer(cmd, attachments[0].view, attachments[0].layout, attachments[0].loadOp, VK_ATTACHMENT_STORE_OP_STORE, vkClear);
}

void Engine::endGraphics(const CommandBuffer& cmd, const PassHandle& passHandle) {
    endDynamicRenderer(cmd);
    const CompiledPass& compiled = graph.compiledPasses[passHandle.id];
    if (compiled.timestampIndex.has_value()) {
        uint32_t base = (currentFrame * numTimestamps + *compiled.timestampIndex) * 2;
        vkCmdWriteTimestamp(cmd.get(), VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestampPool, base + 1);
    }
}

double Engine::getTimestampMs(TimestampHandle handle) const {
    if (handle.id >= cachedTimestamps.size()) return -1.0;
    return cachedTimestamps[handle.id];
}

// ——— Descriptor management ———————————————————————————————————————————————————

void Engine::refreshDescriptorsForImage(uint32_t imageId, uint32_t frameIndex) {
    for (const CompiledPass& pass : graph.compiledPasses) {
        if (!passDescriptorGroups.count(pass.pass.id)) continue;
        for (const DescriptorBindingDecl& decl : pass.descriptorBindings) {
            if (decl.isImage && decl.handleId == imageId) {
                dirtyDescriptors.insert(pass.pass.id * MAX_FRAME_IN_FLIGHT + frameIndex);
                break;
            }
        }
    }
}

void Engine::refreshDescriptorsForBuffer(uint32_t bufferId) {
    for (const CompiledPass& pass : graph.compiledPasses) {
        if (!passDescriptorGroups.count(pass.pass.id)) continue;
        for (const DescriptorBindingDecl& decl : pass.descriptorBindings) {
            if (!decl.isImage && decl.handleId == bufferId) {
                for (uint32_t f = 0; f < MAX_FRAME_IN_FLIGHT; ++f)
                    dirtyDescriptors.insert(pass.pass.id * MAX_FRAME_IN_FLIGHT + f);
                break;
            }
        }
    }
}

void Engine::updateDescriptors(const PassHandle& pass, uint32_t frameIndex) {
    uint32_t passId = pass.id;
    auto groupIt = passDescriptorGroups.find(passId);
    if (groupIt == passDescriptorGroups.end()) return;

    const CompiledPass& compiled = graph.compiledPasses[passId];
    const std::vector<DescriptorSetLayout>& layouts = passDescriptorLayouts.at(passId);
    for (uint32_t set = 0; set < layouts.size(); ++set) {
        if (!groupIt->second[set].isValid()) continue;

        DescriptorWriter writer(layouts[set]);
        for (const DescriptorBindingDecl& decl : compiled.descriptorBindings) {
            if (decl.set != set) continue;
            if (decl.isImage) {
                ImageHandle h = { .id = decl.handleId };
                if (decl.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                    writer.combinedImageSampler(decl.binding, getView(h), getSampler(h));
                else if (decl.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    writer.storageImage(decl.binding, getView(h));
            } else {
                BufferHandle h = { .id = decl.handleId };
                if (decl.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                    writer.uniform(decl.binding, getBuffer(h));
                else if (decl.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
                    writer.storage(decl.binding, getBuffer(h));
            }
        }
        updateDescriptorSetGroup(groupIt->second[set], frameIndex, writer);
    }
}

void Engine::bindDescriptors(const CommandBuffer& cmd, const PassHandle& passHandle) {
    uint32_t passId = passHandle.id;
    auto groupIt = passDescriptorGroups.find(passId);
    if (groupIt == passDescriptorGroups.end()) return;

    auto dirtyIt = dirtyDescriptors.find(passId * MAX_FRAME_IN_FLIGHT + currentFrame);
    if (dirtyIt != dirtyDescriptors.end()) {
        updateDescriptors(passHandle, currentFrame);
        dirtyDescriptors.erase(dirtyIt);
    }

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

    for (uint32_t set = 0; set < groupIt->second.size(); ++set)
        if (groupIt->second[set].isValid()) groupIt->second[set].at(currentFrame).bind(cmd, layout, bindPoint, set);
}

// ——— Barriers ————————————————————————————————————————————————————————————————

void Engine::emitBarriers(const CommandBuffer& commandBuffer, const PassHandle& passHandle) const {
    for (const PassImageBarrier& barrier : graph.compiledPasses[passHandle.id].barriersBefore)
        emitBarrier(commandBuffer, barrier);

    for (const PassBufferBarrier& b : graph.compiledPasses[passHandle.id].bufferBarriersBefore) {
        auto srcVk = bufferAccessToVk(b.srcAccess);
        auto dstVk = bufferAccessToVk(b.dstAccess);
        VkBuffer buf = bufferBindings.at(b.buffer.id);
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

void Engine::emitOutputBarriers(const CommandBuffer& commandBuffer) const {
    for (const PassImageBarrier& barrier : graph.outputBarriers)
        emitBarrier(commandBuffer, barrier);
}

void Engine::emitBarrier(const CommandBuffer& commandBuffer, const PassImageBarrier& barrier) const {
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

std::vector<AttachmentInfo> Engine::getColorAttachment(const PassHandle& passHandle) const {
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

ImageBinding Engine::getTransferSrc(const PassHandle& passHandle) const {
    for (const ImageAccess& access : graph.compiledPasses[passHandle.id].imageAccesses)
        if (access.info.usage == ImageUsageType::TransferSrc)
            return bindings.at(access.image.id);
    throw std::runtime_error("No transfer src image found in pass");
}

ImageBinding Engine::getTransferDst(const PassHandle& passHandle) const {
    for (const ImageAccess& access : graph.compiledPasses[passHandle.id].imageAccesses)
        if (access.info.usage == ImageUsageType::TransferDst)
            return bindings.at(access.image.id);
    throw std::runtime_error("No transfer dst image found in pass");
}

std::string Engine::dumpGraph() const {
    return VkSmol::dumpGraph(graph);
}

}
