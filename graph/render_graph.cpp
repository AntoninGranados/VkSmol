#include "render_graph.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <format>
#include <stdexcept>
#include <utility>

#include "../engine.hpp"
#include "graph_utils.hpp"
#include "graph_validation.hpp"

namespace VkSmol {

RenderGraph::RenderGraph(Engine& engine)
    : engine(engine), device(engine.getDevice()), logicalDevice(engine.getDevice().getLogicalDevice()),
      descriptors(device, engine.getResources()), pipelines(device, engine.getResources()) {}

RenderGraph::~RenderGraph() { release(); }

void RenderGraph::reset() {
    release();
    declaration = {};
    baked = false;
    ownedImages.clear();
    ownedBuffers.clear();
    relativeImages.clear();
    presentedImage.reset();
    acquireWaitStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    recordedSlots = {};
    numTimestamps = 0;
    cachedTimestamps.clear();
}

void RenderGraph::release() {
    device.waitIdle();
    for (const Fence& fence : fences) engine.unregisterFence(fence);
    if (timestampPool != VK_NULL_HANDLE) {
        vkDestroyQueryPool(logicalDevice.get(), timestampPool, nullptr);
        timestampPool = VK_NULL_HANDLE;
    }
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

ImageHandle
RenderGraph::createImage(std::string name, VkFormat format, SwapchainRelative size, uint32_t layers, uint32_t mips) {
    assert(!baked);
    if (engine.isHeadless())
        throw std::runtime_error(std::format("The image [{}] is sized relative to a swapchain the engine lacks", name));
    if (!(size.scale > 0.0f))
        throw std::runtime_error(std::format("The image [{}] needs a positive swapchain scale", name));
    const VkExtent2D extent = relativeExtent(size);
    const ImageHandle image = createImage(std::move(name), format, extent.width, extent.height, layers, mips);
    relativeImages.emplace_back(image, size);
    return image;
}

VkExtent2D RenderGraph::relativeExtent(SwapchainRelative size) const {
    const VkExtent2D extent = engine.getExtent();
    const auto scaled = [&](uint32_t value) {
        return std::max(1u, static_cast<uint32_t>(std::lround(static_cast<float>(value) * size.scale)));
    };
    return {scaled(extent.width), scaled(extent.height)};
}

void RenderGraph::resizeRelativeImages() {
    for (const auto& [image, size] : relativeImages) {
        const VkExtent2D extent = relativeExtent(size);
        engine.resizeImage(image, extent.width, extent.height);
    }
}

BufferHandle RenderGraph::createBuffer(std::string name, size_t size) {
    assert(!baked);
    return ownedBuffers.emplace_back(engine.createBuffer(std::move(name), size));
}

PassHandle RenderGraph::addPass(std::string name, PassType type) {
    assert(!baked);
    const PassHandle handle = {.id = static_cast<uint32_t>(declaration.passes.size())};
    declaration.passes.push_back({.id = handle.id, .name = std::move(name), .type = type});
    return handle;
}

ComputePassBuilder RenderGraph::addComputePass(std::string name) {
    return ComputePassBuilder(addPass(std::move(name), PassType::Compute), &declaration);
}

GraphicsPassBuilder RenderGraph::addGraphicsPass(std::string name) {
    return GraphicsPassBuilder(addPass(std::move(name), PassType::Graphics), &declaration);
}

TransferPassBuilder RenderGraph::addTransferPass(std::string name) {
    return TransferPassBuilder(addPass(std::move(name), PassType::Transfer), &declaration);
}

void RenderGraph::present(ImageHandle image) {
    assert(!baked && !engine.isHeadless() && image.id == engine.getSwapchainImage().id);
    presentedImage = image;
    const PassHandle handle = addPass("Present", PassType::Present);
    declaration.passes[handle.id].imageAccesses.push_back(
        {.image = image, .info = {.usage = ImageUsageType::Present, .access = AccessType::Read}}
    );
}

// ——— Bake ————————————————————————————————————————————————————————————————————

void RenderGraph::bake(std::string pipelineCachePath) {
    assert(!baked);
    std::vector<std::string> errors;

    resolveBindings(
        declaration.passes, declaration.computePipelines, declaration.graphicsPipelines, device, engine.getResources(),
        errors
    );
    checkImageAccesses(declaration.passes, engine.getResources(), errors);
    if (errors.empty())
        checkAttachments(declaration.passes, declaration.graphicsPipelines, engine.getResources(), errors);
    if (!errors.empty()) {
        std::string message = errors.front();
        for (size_t i = 1; i < errors.size(); ++i) message += "\n" + errors[i];
        throw std::runtime_error(message);
    }

    if (presentedImage) {
        for (const PassInfo& pass : declaration.passes) {
            const auto access = std::ranges::find_if(pass.imageAccesses, [&](const ImageAccess& a) { return a.image.id == presentedImage->id; });
            if (access == pass.imageAccesses.end()) continue;
            acquireWaitStage = imageAccessToVk(access->info).stage;
            break;
        }
    }

    descriptors.init(declaration);
    pipelines.init(declaration, descriptors, pipelineCachePath);
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

    numTimestamps = static_cast<uint32_t>(declaration.passes.size());
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
    for (const PassInfo& pass : declaration.passes)
        if (pass.name == passName && pass.id < cachedTimestamps.size())
            return cachedTimestamps[pass.id];
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
    resizeRelativeImages();

    Fence::reset({ &fences[frame] }, logicalDevice);

    CommandBuffer& commandBuffer = commandBuffers[frame];
    commandBuffer.reset();
    commandBuffer.beginRecording();
    engine.pendingTransfers.flush(commandBuffer);
    for (const PassInfo& pass : declaration.passes)
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

void RenderGraph::recordPass(const CommandBuffer& commandBuffer, const PassInfo& pass) {
    const uint32_t base = (frame * numTimestamps + pass.id) * 2;

    vkCmdResetQueryPool(commandBuffer.get(), timestampPool, base, 2);
    logicalDevice.functions().cmdWriteTimestamp2(commandBuffer.get(), VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, timestampPool, base);
    device.beginDebugLabel(commandBuffer, pass.name);
    emitBarriers(commandBuffer, pass);

    switch (pass.type) {
        case PassType::Compute: {
            if (pass.pipelineId) {
                pipelines.bind(commandBuffer, pass);
                descriptors.bind(commandBuffer, pass, frame, pipelines.getLayout(pass));
            }
            PassContext context(*this, commandBuffer, pass, {});
            if (pass.execute) pass.execute(context);
            break;
        }
        case PassType::Graphics:
            recordGraphicsPass(commandBuffer, pass);
            break;
        case PassType::Transfer: {
            PassContext context(*this, commandBuffer, pass, {});
            if (pass.execute) pass.execute(context);
            break;
        }
        case PassType::Present:
            break;
    }

    device.endDebugLabel(commandBuffer);
    logicalDevice.functions().cmdWriteTimestamp2(commandBuffer.get(), VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, timestampPool, base + 1);
}

void RenderGraph::recordGraphicsPass(const CommandBuffer& commandBuffer, const PassInfo& pass) {
    ResourceRegistry& resources = engine.getResources();
    const std::vector<ImageAccess> colors = findColorAttachments(pass.imageAccesses);
    const std::optional<ImageAccess> depth = findDepthAttachment(pass.imageAccesses);

    std::optional<VkExtent2D> extent;
    const auto attach = [&](const ImageAccess& access) {
        const ImageRange range(access.image, access.subresources);
        const VkExtent2D size = engine.getImageExtent(access.image, resources.resolve(range).baseMip);
        if (extent && (extent->width != size.width || extent->height != size.height))
            throw std::runtime_error(
                std::format(
                    "The graphics pass [{}] attaches [{}] of {}x{} to a render area of {}x{}", pass.name,
                    resources.getImage(access.image).name, size.width, size.height, extent->width, extent->height
                )
            );
        extent = size;
        const bool readOnly = access.info.access == AccessType::Read;
        return VkRenderingAttachmentInfo{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = resources.getImageView(range, VK_IMAGE_VIEW_TYPE_2D),
            .imageLayout = imageAccessToVk(access.info).layout,
            .loadOp = readOnly ? VK_ATTACHMENT_LOAD_OP_LOAD : attachmentLoadToVk(access.info.attachmentLoad),
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = access.clearValue,
        };
    };

    std::vector<VkRenderingAttachmentInfo> colorInfos;
    for (const ImageAccess& access : colors) colorInfos.push_back(attach(access));
    const std::optional<VkRenderingAttachmentInfo> depthInfo = depth ? std::optional(attach(*depth)) : std::nullopt;
    if (!extent) throw std::runtime_error(std::format("The graphics pass [{}] has no attachment", pass.name));
    const VkRenderingInfo renderingInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = VkRect2D{{}, *extent},
        .layerCount = 1,
        .colorAttachmentCount = static_cast<uint32_t>(colorInfos.size()),
        .pColorAttachments = colorInfos.data(),
        .pDepthAttachment = depthInfo ? &*depthInfo : nullptr,
    };
    logicalDevice.functions().cmdBeginRendering(commandBuffer.get(), &renderingInfo);

    if (pass.pipelineId) {
        pipelines.bind(commandBuffer, pass);
        pipelines.setRenderArea(commandBuffer, pass, *extent);
        descriptors.bind(commandBuffer, pass, frame, pipelines.getLayout(pass));

        std::vector<VkBuffer> vertexBuffers;
        for (const BufferAccess& access : pass.bufferAccesses) {
            const VkBuffer buffer = resources.getBuffer(access.buffer).buffer;
            if (access.info.usage == BufferUsageType::Vertex) vertexBuffers.push_back(buffer);
            if (access.info.usage == BufferUsageType::Index)
                vkCmdBindIndexBuffer(commandBuffer.get(), buffer, 0, pass.indexType);
        }
        if (!vertexBuffers.empty()) {
            const std::vector<VkDeviceSize> offsets(vertexBuffers.size(), 0);
            vkCmdBindVertexBuffers(
                commandBuffer.get(), 0, static_cast<uint32_t>(vertexBuffers.size()), vertexBuffers.data(),
                offsets.data()
            );
        }
    }
    PassContext context(*this, commandBuffer, pass, *extent);
    if (pass.execute) pass.execute(context);

    logicalDevice.functions().cmdEndRendering(commandBuffer.get());
}

void RenderGraph::emitBarriers(const CommandBuffer& commandBuffer, const PassInfo& pass) {
    ResourceRegistry& resources = engine.getResources();
    for (const ImageAccess& access : pass.imageAccesses)
        resources.transitionImage(commandBuffer, ImageRange(access.image, access.subresources), access.info);
    for (const BufferAccess& access : pass.bufferAccesses)
        resources.transitionBuffer(commandBuffer, access.buffer, access.info);
}

std::string RenderGraph::dump() const { return dumpGraph(declaration, engine); }
}
