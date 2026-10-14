#include "render_graph.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <format>
#include <stdexcept>
#include <utility>

#include "../engine.hpp"
#include "graph_utils.hpp"
#include "graph_validation.hpp"

namespace VkSmol {

RenderGraph::RenderGraph(Engine& engine)
    : engine(engine), device(engine.getDevice()), logicalDevice(engine.getDevice().getLogicalDevice()),
      descriptors(device, engine.getResources()), pipelines(device, engine.getResources()) {
    dynamicRenderer.init(logicalDevice);
}

RenderGraph::~RenderGraph() {
    device.waitIdle();
    for (const Fence& fence : fences) engine.unregisterFence(fence);
    if (timestampPool != VK_NULL_HANDLE)
        vkDestroyQueryPool(logicalDevice.get(), timestampPool, nullptr);
    if (baked) {
        destroySync();
        pipelines.destroy();
    }
    descriptors.destroy();
    for (ImageHandle image : ownedImages) engine.destroyImage(image);
    for (BufferHandle buffer : ownedBuffers) engine.destroyBuffer(buffer);
}

// ——— Declaration —————————————————————————————————————————————————————————————

ImageHandle RenderGraph::createImage(
    std::string name, VkFormat format, uint32_t width, uint32_t height, uint32_t layers, uint32_t mips
) {
    assert(!baked);
    return ownedImages.emplace_back(engine.createImage(std::move(name), format, width, height, layers, mips));
}

BufferHandle RenderGraph::createBuffer(std::string name, size_t size) {
    assert(!baked);
    return ownedBuffers.emplace_back(engine.createBuffer(std::move(name), size));
}

ComputePassBuilder RenderGraph::addComputePass(std::string name) {
    assert(!baked);
    return builder.addComputePass(std::move(name));
}

GraphicsPassBuilder RenderGraph::addGraphicsPass(std::string name) {
    assert(!baked);
    return builder.addGraphicsPass(std::move(name));
}

TransferPassBuilder RenderGraph::addTransferPass(std::string name) {
    assert(!baked);
    return builder.addTransferPass(std::move(name));
}

void RenderGraph::present(ImageHandle image) {
    assert(!baked && !engine.isHeadless() && image.id == engine.getSwapchainImage().id);
    presentedImage = image;
    builder.addPresentPass(image);
}

// ——— Bake ————————————————————————————————————————————————————————————————————

void RenderGraph::bake(std::string pipelineCachePath) {
    assert(!baked);
    std::vector<std::string> errors;

    resolveBindings(
        builder.passes, builder.computePipelines, builder.graphicsPipelines, device, engine.getResources(), errors
    );
    checkImageAccesses(builder.passes, engine.getResources(), errors);
    if (!errors.empty()) {
        std::string message = errors.front();
        for (size_t i = 1; i < errors.size(); ++i) message += "\n" + errors[i];
        throw std::runtime_error(message);
    }
    graph = builder.compile();

    if (presentedImage) {
        for (const CompiledPass& pass : graph.compiledPasses) {
            const auto access = std::ranges::find_if(pass.imageAccesses, [&](const ImageAccess& a) { return a.image.id == presentedImage->id; });
            if (access == pass.imageAccesses.end()) continue;
            acquireWaitStage = imageAccessToVk(access->info).stage;
            break;
        }
    }

    descriptors.init(graph);
    pipelines.init(graph, descriptors, pipelineCachePath);
    initSync();
    initTimestamps();

    baked = true;
}

// ——— Sync ————————————————————————————————————————————————————————————————————

void RenderGraph::initSync() {
    for (uint32_t f = 0; f < MAX_FRAME_IN_FLIGHT; ++f) {
        commandBuffers[f] = device.allocCommandBuffer();
        fences[f].init(logicalDevice, true);
    }
}

void RenderGraph::destroySync() {
    for (uint32_t f = 0; f < MAX_FRAME_IN_FLIGHT; ++f) {
        device.freeCommandBuffer(commandBuffers[f]);
        fences[f].destroy(logicalDevice);
    }
}

// ——— Timestamps ——————————————————————————————————————————————————————————————

void RenderGraph::initTimestamps() {
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(device.getPhysicalDevice().get(), &props);
    timestampPeriod = props.limits.timestampPeriod;

    numTimestamps = static_cast<uint32_t>(graph.compiledPasses.size());
    if (numTimestamps == 0) return;

    VkQueryPoolCreateInfo info{};
    info.sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    info.queryType  = VK_QUERY_TYPE_TIMESTAMP;
    info.queryCount = numTimestamps * 2 * MAX_FRAME_IN_FLIGHT;
    vkCheck(vkCreateQueryPool(logicalDevice.get(), &info, nullptr, &timestampPool), "Failed to create timestamp query pool");
}

void RenderGraph::readTimestamps() {
    if (!timestampPool || !recordedSlots[frame]) return;
    cachedTimestamps.resize(numTimestamps, -1.0);
    for (uint32_t i = 0; i < numTimestamps; ++i) {
        uint32_t base = (frame * numTimestamps + i) * 2;
        uint64_t data[4] = {};
        vkGetQueryPoolResults(
            logicalDevice.get(), timestampPool, base, 2,
            sizeof(data), data, 2 * sizeof(uint64_t),
            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT
        );
        cachedTimestamps[i] = (data[1] && data[3])
            ? static_cast<double>(data[2] - data[0]) * timestampPeriod / 1e6
            : -1.0;
    }
}

double RenderGraph::getTimestampMs(const std::string& passName) const {
    for (const CompiledPass& pass : graph.compiledPasses)
        if (pass.name == passName && pass.pass.id < cachedTimestamps.size())
            return cachedTimestamps[pass.pass.id];
    return -1.0;
}

// ——— Execution ———————————————————————————————————————————————————————————————

void RenderGraph::execute() {
    if (!baked) throw std::runtime_error("RenderGraph::execute() called before a successful bake()");

    frame = engine.currentFrame;
    engine.beginSlot();
    Fence::wait({ &fences[frame] }, logicalDevice);
    readTimestamps();

    std::optional<uint32_t> imageIndex;
    if (presentedImage) {
        imageIndex = engine.acquireImage();
        if (!imageIndex) return;
    }

    Fence::reset({ &fences[frame] }, logicalDevice);

    CommandBuffer& commandBuffer = commandBuffers[frame];
    commandBuffer.reset();
    commandBuffer.beginRecording();
    engine.pendingTransfers.flush(commandBuffer);
    for (const CompiledPass& pass : graph.compiledPasses)
        recordPass(commandBuffer, pass);
    commandBuffer.endRecording();
    recordedSlots[frame] = true;

    std::vector<const Semaphore*> waitSemaphores, signalSemaphores;
    std::vector<VkPipelineStageFlags2> waitStages;
    if (presentedImage) {
        waitSemaphores.push_back(&engine.imageAvailableSemaphores[frame]);
        waitStages.push_back(acquireWaitStage);
        signalSemaphores.push_back(&engine.renderFinishedSemaphores[*imageIndex]);
    }
    device.getGraphicsQueue().submit({ &commandBuffer }, fences[frame], waitSemaphores, waitStages, signalSemaphores);
    engine.registerFence(frame, fences[frame]);

    if (presentedImage) engine.presentImage(*imageIndex);
}

void RenderGraph::recordPass(const CommandBuffer& commandBuffer, const CompiledPass& pass) {
    const PassInfo& info = builder.passes[pass.pass.id];
    const uint32_t base = (frame * numTimestamps + pass.pass.id) * 2;

    vkCmdResetQueryPool(commandBuffer.get(), timestampPool, base, 2);
    logicalDevice.functions().cmdWriteTimestamp2(commandBuffer.get(), VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, timestampPool, base);
    emitBarriers(commandBuffer, pass);

    switch (pass.type) {
        case PassType::Compute: {
            if (pass.pipelineId) {
                pipelines.bind(commandBuffer, pass);
                descriptors.bind(commandBuffer, pass, frame, pipelines.getLayout(pass));
            }
            PassContext context(*this, commandBuffer, pass, {});
            if (info.execute) info.execute(context);
            break;
        }
        case PassType::Graphics:
            recordGraphicsPass(commandBuffer, pass, info);
            break;
        case PassType::Transfer: {
            PassContext context(*this, commandBuffer, pass, {});
            if (info.execute) info.execute(context);
            break;
        }
        case PassType::Present:
            break;
    }

    logicalDevice.functions().cmdWriteTimestamp2(commandBuffer.get(), VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, timestampPool, base + 1);
}

void RenderGraph::recordGraphicsPass(
    const CommandBuffer& commandBuffer, const CompiledPass& pass, const PassInfo& info
) {
    const std::optional<ImageAccess> attachment = findColorAttachment(pass);
    if (!attachment) throw std::runtime_error(std::format("The graphics pass [{}] has no color attachment", pass.name));

    ResourceRegistry& resources = engine.getResources();
    const ImageRange target(attachment->image, attachment->subresources);
    const VkExtent2D extent = engine.getImageExtent(attachment->image, resources.resolve(target).baseMip);
    VkClearColorValue clearColor = {};
    std::memcpy(clearColor.float32, info.clearColor.data(), sizeof(clearColor.float32));
    dynamicRenderer.begin(
        commandBuffer, resources.getImageView(target, VK_IMAGE_VIEW_TYPE_2D), imageAccessToVk(attachment->info).layout,
        attachmentLoadToVk(attachment->info.attachmentLoad), VK_ATTACHMENT_STORE_OP_STORE, clearColor,
        VkRect2D{{}, extent}
    );

    if (pass.pipelineId) {
        pipelines.bind(commandBuffer, pass);
        pipelines.setRenderArea(commandBuffer, pass, extent);
        descriptors.bind(commandBuffer, pass, frame, pipelines.getLayout(pass));

        std::vector<VkBuffer> vertexBuffers;
        for (const BufferAccess& access : pass.bufferAccesses) {
            const VkBuffer buffer = resources.getBuffer(access.buffer).buffer;
            if (access.info.usage == BufferUsageType::Vertex) vertexBuffers.push_back(buffer);
            if (access.info.usage == BufferUsageType::Index)
                vkCmdBindIndexBuffer(commandBuffer.get(), buffer, 0, info.indexType);
        }
        if (!vertexBuffers.empty()) {
            const std::vector<VkDeviceSize> offsets(vertexBuffers.size(), 0);
            vkCmdBindVertexBuffers(
                commandBuffer.get(), 0, static_cast<uint32_t>(vertexBuffers.size()), vertexBuffers.data(),
                offsets.data()
            );
        }
    }
    PassContext context(*this, commandBuffer, pass, extent);
    if (info.execute) info.execute(context);

    dynamicRenderer.end(commandBuffer);
}

void RenderGraph::emitBarriers(const CommandBuffer& commandBuffer, const CompiledPass& pass) {
    ResourceRegistry& resources = engine.getResources();
    for (const ImageAccess& access : pass.imageAccesses)
        resources.transitionImage(commandBuffer, ImageRange(access.image, access.subresources), access.info);
    for (const BufferAccess& access : pass.bufferAccesses)
        resources.transitionBuffer(commandBuffer, access.buffer, access.info);
}

std::string RenderGraph::dump() const { return dumpGraph(graph, engine); }
}
