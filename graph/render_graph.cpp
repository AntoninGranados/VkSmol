#include "render_graph.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <format>
#include <memory>
#include <stdexcept>
#include <utility>

#include "graph_utils.hpp"
#include "../engine.hpp"
#include "../image/image_barrier.hpp"

namespace VkSmol {

RenderGraph::RenderGraph(Engine& engine)
    : engine(engine), device(engine.getDevice()), logicalDevice(engine.getDevice().getLogicalDevice()) {
    dynamicRenderer.init(logicalDevice);
}

RenderGraph::~RenderGraph() {
    device.waitIdle();
    if (timestampPool != VK_NULL_HANDLE)
        vkDestroyQueryPool(logicalDevice.get(), timestampPool, nullptr);
    if (baked) {
        destroySync();
        destroyPipelines();
        pipelineCache.destroy(logicalDevice);
    }
    dirtyDescriptors.clear();
    destroyDescriptorSets();
    discardBufferTransfers();
    destroyBuffers();
    destroyImages();
    for (StagingArena& arena : stagingArenas) device.destroyBuffer(arena.buffer);
    device.waitIdle();
}

// ——— Declaration —————————————————————————————————————————————————————————————

ImageHandle RenderGraph::createImage(std::string name, VkFormat format, uint32_t width, uint32_t height) {
    assert(!baked);
    return builder.createImage(std::move(name), format, width, height);
}

BufferHandle RenderGraph::createBuffer(std::string name, size_t size, const void* initialData) {
    assert(!baked);
    return builder.createBuffer(std::move(name), size, BufferOwnership::Managed, 0, initialData);
}

ImageHandle RenderGraph::importSwapchain() {
    assert(!baked && !engine.isHeadless());
    if (swapchainImage) return *swapchainImage;
    const VkExtent2D extent = engine.getExtent();
    swapchainImage = builder.createImage(
        "Swapchain", engine.swapchain->getImageFormat(), extent.width, extent.height, 1,
        ImageOwnership::Imported, 0,
        ImageAccessInfo{ .usage = ImageUsageType::Undefined, .access = AccessType::None },
        ImageAccessInfo{ .usage = ImageUsageType::Present, .access = AccessType::Read }
    );
    return *swapchainImage;
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
    assert(!baked && swapchainImage && image.id == swapchainImage->id);
    presentedImage = image;
    builder.addPresentPass(image);
}

// ——— Bake ————————————————————————————————————————————————————————————————————

std::vector<std::string> RenderGraph::bake(std::string pipelineCachePath) {
    assert(!baked);
    std::vector<std::string> errors;

    resolveBindings(errors);
    if (!errors.empty()) return errors;

    try {
        graph = builder.compile();
    } catch (const std::exception& e) {
        errors.push_back(e.what());
        return errors;
    }

    if (swapchainImage) {
        for (const CompiledPass& pass : graph.compiledPasses) {
            const auto access = std::ranges::find_if(pass.imageAccesses, [&](const ImageAccess& a) { return a.image.id == swapchainImage->id; });
            if (access == pass.imageAccesses.end()) continue;
            acquireWaitStage = imageAccessToVk(access->info).stage;
            break;
        }
    }

    pipelineCache.init(logicalDevice, pipelineCachePath);
    initImages();
    initBuffers();
    initDescriptorSets();
    try {
        std::vector<std::string> pipelineErrors = initPipelines();
        errors.insert(errors.end(), pipelineErrors.begin(), pipelineErrors.end());
    } catch (const std::exception& e) {
        errors.push_back(e.what());
    }
    device.markShadersStale();
    initSync();
    for (const CompiledPass& pass : graph.compiledPasses)
        if (passDescriptorGroups.count(pass.pass.id))
            for (uint32_t f = 0; f < MAX_FRAME_IN_FLIGHT; ++f)
                updateDescriptors(pass.pass, f);
    initTimestamps();

    baked = true;
    return errors;
}

void RenderGraph::resolveBindings(std::vector<std::string>& errors) {
    for (uint32_t passId = 0; passId < builder.passes.size(); ++passId) {
        PassInfo& pass = builder.passes[passId];
        if (pass.namedBindings.empty()) continue;

        std::vector<std::pair<std::string, ShaderDefines>> shaders;
        VkShaderStageFlags stageFlags = 0;
        for (const ComputePipelineInfo& info : builder.computePipelines)
            if (info.passId == passId) {
                shaders.emplace_back(info.shaderPath, info.shaderDefines);
                stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            }
        for (const GraphicsPipelineInfo& info : builder.graphicsPipelines)
            if (info.passId == passId) {
                for (const auto& [stage, path] : info.shaderPaths) shaders.emplace_back(path, info.shaderDefines);
                stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
            }
        if (shaders.empty()) {
            errors.push_back(std::format("The pass [{}] binds shader parameters but has no pipeline", pass.name));
            continue;
        }

        std::vector<const ShaderParameter*> parameters;
        try {
            for (const auto& [path, defines] : shaders)
                for (const ShaderParameter& parameter : device.compileShader(path, defines).compilation.parameters)
                    parameters.push_back(&parameter);
        } catch (const std::exception& e) {
            errors.push_back(e.what());
            continue;
        }

        for (const NamedBinding& binding : pass.namedBindings) {
            const auto found = std::ranges::find_if(parameters, [&](const ShaderParameter* p) { return p->name == binding.name; });
            if (found == parameters.end()) {
                errors.push_back(std::format("The shaders of the pass [{}] have no parameter named [{}{}{}]", pass.name, MSG_RED_B, binding.name, MSG_RESET));
                continue;
            }
            const ShaderParameter& parameter = **found;
            if (binding.access == AccessType::Write && !parameter.writable) {
                errors.push_back(std::format("The pass [{}] writes [{}{}{}] but the shader declares it read-only", pass.name, MSG_RED_B, binding.name, MSG_RESET));
                continue;
            }

            if (binding.isImage) {
                std::optional<ImageUsageType> usage;
                if (parameter.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) usage = ImageUsageType::Storage;
                if (parameter.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) usage = ImageUsageType::Sampled;
                if (!usage) {
                    errors.push_back(std::format("The parameter [{}{}{}] of the pass [{}] is not an image the graph can bind", MSG_RED_B, binding.name, MSG_RESET, pass.name));
                    continue;
                }
                pass.imageAccesses.push_back({ .image = { .id = binding.handleId }, .info = { .usage = *usage, .access = binding.access } });
            } else {
                std::optional<BufferUsageType> usage;
                if (parameter.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) usage = BufferUsageType::Storage;
                if (parameter.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) usage = BufferUsageType::Uniform;
                if (!usage) {
                    errors.push_back(std::format("The parameter [{}{}{}] of the pass [{}] is not a buffer the graph can bind", MSG_RED_B, binding.name, MSG_RESET, pass.name));
                    continue;
                }
                pass.bufferAccesses.push_back({ .buffer = { .id = binding.handleId }, .info = { .usage = *usage, .access = binding.access } });
            }

            pass.descriptorBindings.push_back({
                .set            = parameter.location.set,
                .binding        = parameter.location.binding,
                .isImage        = binding.isImage,
                .handleId       = binding.handleId,
                .descriptorType = parameter.descriptorType,
                .stageFlags     = stageFlags,
            });
        }
    }
}

void RenderGraph::initImages() {
    for (uint32_t id = 0; id < graph.images.size(); ++id) {
        const ImageInfo& info = graph.images[id];
        if (info.ownership == ImageOwnership::Imported || ownedImages.contains(id)) continue;

        Image img  = device.createImage(info.width, info.height, info.format, imageUsageFlags(graph, id), VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, info.name);
        ImageView view = device.createImageView(img, info.name + "View");
        Sampler   smp  = device.createSampler(info.name + "Sampler");

        bindings[id] = { .image = img.get(), .view = view.get() };
        ownedImages.emplace(id, std::move(img));
        ownedViews.emplace(id, std::move(view));
        ownedSamplers.emplace(id, std::move(smp));
    }
}

void RenderGraph::destroyImages() {
    for (auto& [id, img] : ownedImages) {
        device.destroySampler(ownedSamplers.at(id));
        device.destroyImageView(ownedViews.at(id));
        device.destroyImage(img);
        bindings.erase(id);
    }
    ownedSamplers.clear();
    ownedViews.clear();
    ownedImages.clear();
}

VkImageUsageFlags RenderGraph::imageUsageFlags(const CompiledRenderGraph& compiled, uint32_t imageId) const {
    VkImageUsageFlags usageFlags = compiled.images[imageId].extraUsageFlags;
    for (const CompiledPass& pass : compiled.compiledPasses)
        for (const ImageAccess& access : pass.imageAccesses)
            if (access.image.id == imageId)
                usageFlags |= imageUsageTypeToVkFlags(access.info.usage);
    return usageFlags;
}

// ——— Buffers —————————————————————————————————————————————————————————————————

void RenderGraph::initBuffers() {
    for (uint32_t id = 0; id < graph.buffers.size(); ++id) {
        const BufferInfo& info = graph.buffers[id];
        if (info.ownership == BufferOwnership::Imported || ownedBuffers.contains(id)) continue;

        void* initialData = info.initialData.empty() ? nullptr : (void*)info.initialData.data();
        Buffer buf = device.createBuffer(bufferUsageFlags(graph, id), info.size, 0, initialData, info.name);
        bufferBindings[id] = buf.get();
        ownedBuffers.emplace(id, std::move(buf));
    }
}

void RenderGraph::destroyBuffers() {
    for (auto& [id, buf] : ownedBuffers) {
        device.destroyBuffer(buf);
        bufferBindings.erase(id);
    }
    ownedBuffers.clear();
}

VkBufferUsageFlags RenderGraph::bufferUsageFlags(const CompiledRenderGraph& compiled, uint32_t bufferId) const {
    VkBufferUsageFlags usageFlags = compiled.buffers[bufferId].extraUsageFlags
        | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    for (const CompiledPass& pass : compiled.compiledPasses)
        for (const BufferAccess& access : pass.bufferAccesses)
            if (access.buffer.id == bufferId)
                usageFlags |= bufferUsageTypeToVkFlags(access.info.usage);
    return usageFlags;
}

void RenderGraph::replaceBuffer(uint32_t bufferId, size_t newSize, bool preserve) {
    BufferInfo& info = graph.buffers[bufferId];
    Buffer next = device.createBuffer(bufferUsageFlags(graph, bufferId), newSize, 0, nullptr, info.name);

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

size_t RenderGraph::stage(const void* data, size_t size) {
    StagingArena& arena = stagingArenas[frame];
    size_t offset = (arena.used + 15) & ~size_t(15);
    if (offset + size > arena.buffer.getSize()) {
        const size_t capacity = std::max(arena.buffer.getSize() * 2, size);
        device.destroyBuffer(arena.buffer);
        arena.buffer = device.createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, capacity, BufferAllocMapped, nullptr, "StagingBuffer");
        offset = 0;
    }
    arena.buffer.write(logicalDevice, data, size, offset);
    arena.used = offset + size;
    return offset;
}

void RenderGraph::flushBufferTransfers(const CommandBuffer& commandBuffer) {
    if (pendingBufferTransfers.empty()) return;

    constexpr VkAccessFlags2 transferAccess = VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT;
    device.emitMemoryBarrier(commandBuffer,
        VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, transferAccess);

    struct WrittenRange { VkBuffer buffer; size_t begin, end; };
    std::vector<WrittenRange> written;
    auto serializeTransfers = [&] {
        device.emitMemoryBarrier(commandBuffer,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT, transferAccess);
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
            vkCmdCopyBuffer(commandBuffer.get(), stagingArenas[frame].buffer.get(), write->dst, 1, &region);
            written.push_back({ write->dst, write->offset, end });
        } else {
            auto& replace = std::get<PendingBufferReplace>(transfer);
            if (replace.preservedSize > 0) {
                serializeTransfers();
                const VkBufferCopy region{ .srcOffset = 0, .dstOffset = 0, .size = replace.preservedSize };
                vkCmdCopyBuffer(commandBuffer.get(), replace.previous.get(), replace.next, 1, &region);
                serializeTransfers();
            }
            device.destroyBuffer(replace.previous);
        }
    }
    pendingBufferTransfers.clear();

    device.emitMemoryBarrier(commandBuffer,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT);
}

void RenderGraph::discardBufferTransfers() {
    for (auto& transfer : pendingBufferTransfers)
        if (auto* replace = std::get_if<PendingBufferReplace>(&transfer))
            device.destroyBuffer(replace->previous);
    pendingBufferTransfers.clear();
}

// ——— Descriptor sets —————————————————————————————————————————————————————————

void RenderGraph::initDescriptorSets() {
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
            layouts[set].init(logicalDevice);
            if (!layouts[set].getBindings().empty())
                groups[set] = createDescriptorSetGroup(layouts[set], std::format("{}Descriptors{}", pass.name, set));
        }
    }
}

void RenderGraph::destroyDescriptorSets() {
    for (auto& [id, groups] : passDescriptorGroups)
        for (DescriptorSetGroup& group : groups)
            if (group.isValid()) destroyDescriptorSetGroup(group);
    passDescriptorGroups.clear();
    for (auto& [id, layouts] : passDescriptorLayouts)
        for (DescriptorSetLayout& layout : layouts)
            layout.destroy(logicalDevice);
    passDescriptorLayouts.clear();
}

DescriptorSetGroup RenderGraph::createDescriptorSetGroup(DescriptorSetLayout& layout, std::string name) {
    const std::string baseName = name.empty() ? "DescriptorSet" : std::move(name);

    DescriptorPool pool;
    for (const auto& b : layout.getBindings())
        pool.addPoolSize(b.descriptorType, MAX_FRAME_IN_FLIGHT);
    pool.init(logicalDevice, MAX_FRAME_IN_FLIGHT);

    std::vector<DescriptorSet> sets(MAX_FRAME_IN_FLIGHT);
    for (size_t i = 0; i < MAX_FRAME_IN_FLIGHT; ++i) {
        sets[i].alloc(logicalDevice, pool, layout);
        sets[i].setResourceName(baseName + '[' + std::to_string(i) + ']');
    }
    return DescriptorSetGroup(std::move(pool), std::move(sets));
}

void RenderGraph::updateDescriptorSetGroup(DescriptorSetGroup& group, uint32_t frameIndex, const DescriptorWriter& writer) {
    DescriptorSet& set = group.at(frameIndex);
    set.clear();
    for (const DescriptorWrite& write : writer.getWrites()) {
        std::visit([&](const auto& payload) {
            using T = std::decay_t<decltype(payload)>;
            if constexpr (std::is_same_v<T, UniformBufferPayload>) {
                assert(payload.buffer);
                set.linkUniformBuffer(*payload.buffer, write.binding);
            } else if constexpr (std::is_same_v<T, StorageBufferPayload>) {
                assert(payload.buffer);
                set.linkStorageBuffer(*payload.buffer, write.binding);
            } else if constexpr (std::is_same_v<T, CombinedImageSamplerPayload>) {
                assert(payload.view && payload.sampler);
                set.linkCombinedImageSampler(*payload.view, *payload.sampler, write.binding);
            } else if constexpr (std::is_same_v<T, StorageImagePayload>) {
                assert(payload.view);
                set.linkStorageImage(*payload.view, write.binding);
            }
        }, write.payload);
    }
    set.update(logicalDevice);
}

void RenderGraph::destroyDescriptorSetGroup(DescriptorSetGroup& group) {
    if (!group.isValid()) return;
    auto retired = std::make_shared<DescriptorSetGroup>(std::move(group));
    device.retire([retired](const LogicalDevice& d) { retired->destroy(d); });
    group = {};
}

// ——— Pipelines ———————————————————————————————————————————————————————————————

std::vector<std::string> RenderGraph::initPipelines() {
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
        shader.init(logicalDevice, VK_SHADER_STAGE_COMPUTE_BIT, compilePassShader(graph.compiledPasses[info.passId], VK_SHADER_STAGE_COMPUTE_BIT, info.shaderPath, info.shaderDefines, errors));
        const uint32_t pushConstantSize = device.compileShader(info.shaderPath, info.shaderDefines).compilation.pushConstantSize;

        ComputePipeline pipeline;
        pipeline.init(logicalDevice, shader, layoutsOf(info.passId), pushConstantSize, pipelineCache);
        ownedComputePipelines[id] = std::move(pipeline);
        computePushConstantSizes[id] = pushConstantSize;
        shader.destroy(logicalDevice);
    }

    for (uint32_t id = 0; id < graph.graphicsPipelines.size(); ++id) {
        const GraphicsPipelineInfo& info = graph.graphicsPipelines[id];
        const CompiledPass& pass = graph.compiledPasses[info.passId];
        const std::optional<ImageAccess> attachment = findColorAttachment(pass);
        if (!attachment)
            throw std::runtime_error(std::format("The graphics pass [{}] has a pipeline but no color attachment", pass.name));

        const VkPipelineVertexInputStateCreateInfo vertexInput{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        };

        std::vector<Shader> shaders(info.shaderPaths.size());
        std::vector<const Shader*> shaderPointers;
        uint32_t pushConstantSize = 0;
        for (size_t i = 0; i < info.shaderPaths.size(); ++i) {
            const auto& [stage, path] = info.shaderPaths[i];
            shaders[i].init(logicalDevice, stage, compilePassShader(pass, stage, path, info.shaderDefines, errors));
            shaderPointers.push_back(&shaders[i]);
            pushConstantSize = std::max(pushConstantSize, device.compileShader(path, info.shaderDefines).compilation.pushConstantSize);
        }

        const VkFormat format = graph.images[attachment->image.id].format;
        VkPipelineRenderingCreateInfoKHR rendering{};
        rendering.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        rendering.colorAttachmentCount    = 1;
        rendering.pColorAttachmentFormats = &format;

        GraphicsPipeline pipeline;
        pipeline.init(logicalDevice, rendering, vertexInput, shaderPointers, layoutsOf(info.passId), pushConstantSize, pipelineCache);
        ownedGraphicsPipelines[id] = std::move(pipeline);
        graphicsPushConstantSizes[id] = pushConstantSize;
        for (Shader& shader : shaders) shader.destroy(logicalDevice);
    }

    return errors;
}

void RenderGraph::destroyPipelines() {
    for (auto& [id, pipeline] : ownedComputePipelines)
        pipeline.destroy(logicalDevice);
    ownedComputePipelines.clear();
    for (auto& [id, pipeline] : ownedGraphicsPipelines)
        pipeline.destroy(logicalDevice);
    ownedGraphicsPipelines.clear();
}

const std::vector<uint32_t>& RenderGraph::compilePassShader(const CompiledPass& pass, VkShaderStageFlagBits stage, const std::string& path, const ShaderDefines& defines, std::vector<std::string>& errors) {
    const Device::CompiledShader& compiled = device.compileShader(path, defines);
    if (!compiled.error.empty())
        errors.push_back(compiled.error);

    if (compiled.compilation.stage != stage) {
        throw std::runtime_error(std::format("The shader [{}{}{}] does not have the stage expected by the pass [{}]", MSG_RED_B, path, MSG_RESET, pass.name));
    }

    for (const ShaderParameter& parameter : compiled.compilation.parameters) {
        if (!parameter.used) continue;

        const bool bound = std::ranges::any_of(pass.descriptorBindings, [&](const DescriptorBindingDecl& decl) {
            return decl.set == parameter.location.set && decl.binding == parameter.location.binding;
        });
        if (!bound) {
            throw std::runtime_error(std::format(
                "The pass [{}] does not bind the parameter [{}{}{}] (set {}, binding {}) used by the shader [{}]",
                pass.name, MSG_RED_B, parameter.name, MSG_RESET, parameter.location.set, parameter.location.binding, path
            ));
        }
    }
    return compiled.compilation.spirv;
}

void RenderGraph::resizeImage(ImageHandle handle, uint32_t width, uint32_t height) {
    uint32_t id = handle.id;
    ImageInfo& info = graph.images[id];
    if (info.ownership == ImageOwnership::Imported) return;

    device.destroySampler(ownedSamplers.at(id));
    device.destroyImageView(ownedViews.at(id));
    device.destroyImage(ownedImages.at(id));
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

    Image     img  = device.createImage(width, height, info.format, usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, info.name);
    ImageView view = device.createImageView(img, info.name + "View");
    Sampler   smp  = device.createSampler(info.name + "Sampler");

    bindings[id] = { .image = img.get(), .view = view.get() };
    ownedImages.emplace(id, std::move(img));
    ownedViews.emplace(id, std::move(view));
    ownedSamplers.emplace(id, std::move(smp));

    for (uint32_t f = 0; f < MAX_FRAME_IN_FLIGHT; ++f)
        refreshDescriptorsForImage(id, f);
}


const ImageView& RenderGraph::getView(ImageHandle handle) const {
    return ownedViews.at(handle.id);
}

const Sampler& RenderGraph::getSampler(ImageHandle handle) const {
    return ownedSamplers.at(handle.id);
}

bool RenderGraph::resizeBuffer(BufferHandle handle, size_t newSize) {
    const BufferInfo& info = graph.buffers[handle.id];
    if (info.ownership == BufferOwnership::Imported || info.size >= newSize) return false;
    replaceBuffer(handle.id, newSize, false);
    return true;
}

void RenderGraph::writeBuffer(BufferHandle handle, const void* data, size_t size, size_t offset) {
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

void RenderGraph::readBuffer(BufferHandle handle, void* out, size_t size, size_t offset) {
    if (size == 0) return;
    Buffer readback = device.createBuffer(VK_BUFFER_USAGE_TRANSFER_DST_BIT, size, BufferAllocMapped, nullptr,
                                   graph.buffers[handle.id].name + "Readback");

    device.immediateSubmit([&](const CommandBuffer& commandBuffer) {
        flushBufferTransfers(commandBuffer);
        device.emitMemoryBarrier(commandBuffer,
            VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
        const VkBufferCopy region{ .srcOffset = offset, .dstOffset = 0, .size = size };
        vkCmdCopyBuffer(commandBuffer.get(), bufferBindings.at(handle.id), readback.get(), 1, &region);
        device.emitMemoryBarrier(commandBuffer,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT);
    });

    readback.read(logicalDevice, out, size);
    readback.free(logicalDevice);
    readback.destroy(logicalDevice);
}

// ——— Descriptor management ———————————————————————————————————————————————————

void RenderGraph::refreshDescriptorsForImage(uint32_t imageId, uint32_t frameIndex) {
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

void RenderGraph::refreshDescriptorsForBuffer(uint32_t bufferId) {
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

void RenderGraph::updateDescriptors(const PassHandle& pass, uint32_t frameIndex) {
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
                    writer.uniform(decl.binding, ownedBuffers.at(h.id));
                else if (decl.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
                    writer.storage(decl.binding, ownedBuffers.at(h.id));
            }
        }
        updateDescriptorSetGroup(groupIt->second[set], frameIndex, writer);
    }
}

// ——— Barriers ————————————————————————————————————————————————————————————————

void RenderGraph::emitBarriers(const CommandBuffer& commandBuffer, const CompiledPass& pass) const {
    for (const PassImageBarrier& barrier : pass.barriersBefore)
        emitBarrier(commandBuffer, barrier);

    for (const PassBufferBarrier& b : pass.bufferBarriersBefore) {
        auto srcVk = bufferAccessToVk(b.srcAccess);
        auto dstVk = bufferAccessToVk(b.dstAccess);
        VkBuffer buf = bufferBindings.at(b.buffer.id);
        const VkBufferMemoryBarrier2 barrier{
            .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .srcStageMask        = srcVk.stage,
            .srcAccessMask       = srcVk.access,
            .dstStageMask        = dstVk.stage,
            .dstAccessMask       = dstVk.access,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer              = buf,
            .offset              = 0,
            .size                = VK_WHOLE_SIZE,
        };
        const VkDependencyInfo dependency{
            .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .bufferMemoryBarrierCount = 1,
            .pBufferMemoryBarriers    = &barrier,
        };
        logicalDevice.functions().cmdPipelineBarrier2(commandBuffer.get(), &dependency);
    }
}

void RenderGraph::emitOutputBarriers(const CommandBuffer& commandBuffer) const {
    for (const PassImageBarrier& barrier : graph.outputBarriers)
        emitBarrier(commandBuffer, barrier);
}

void RenderGraph::emitBarrier(const CommandBuffer& commandBuffer, const PassImageBarrier& barrier) const {
    ImageBinding binding = bindings.at(barrier.image.id);
    VulkanImageState src = imageAccessToVk(barrier.srcAccess);
    VulkanImageState dst = imageAccessToVk(barrier.dstAccess);
    ImageBarrier b;
    b.call(logicalDevice, commandBuffer, binding.image,
           src.layout, dst.layout,
           src.access, dst.access,
           src.stage, dst.stage,
           dst.aspect);
}

// ——— Sync ————————————————————————————————————————————————————————————————————

void RenderGraph::initSync() {
    for (uint32_t f = 0; f < MAX_FRAME_IN_FLIGHT; ++f) {
        commandBuffers[f] = device.allocCommandBuffer();
        fences[f].init(logicalDevice, true);
        renderFinishedSemaphores[f].init(logicalDevice);
    }
}

void RenderGraph::destroySync() {
    for (uint32_t f = 0; f < MAX_FRAME_IN_FLIGHT; ++f) {
        device.freeCommandBuffer(commandBuffers[f]);
        fences[f].destroy(logicalDevice);
        renderFinishedSemaphores[f].destroy(logicalDevice);
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
    Fence::wait({ &fences[frame] }, logicalDevice);
    readTimestamps();

    std::optional<uint32_t> imageIndex;
    if (presentedImage) {
        device.collectRetired(frame);
        imageIndex = engine.acquireImage();
        if (!imageIndex) return;
        bindings[swapchainImage->id] = {
            .image = engine.swapchain->getImage(*imageIndex).get(),
            .view  = engine.swapchain->getImageView(*imageIndex).get(),
        };
    }

    Fence::reset({ &fences[frame] }, logicalDevice);
    stagingArenas[frame].used = 0;

    CommandBuffer& commandBuffer = commandBuffers[frame];
    commandBuffer.reset();
    commandBuffer.beginRecording();
    flushBufferTransfers(commandBuffer);
    for (const CompiledPass& pass : graph.compiledPasses)
        recordPass(commandBuffer, pass);
    emitOutputBarriers(commandBuffer);
    commandBuffer.endRecording();
    recordedSlots[frame] = true;

    std::vector<const Semaphore*> waitSemaphores, signalSemaphores;
    std::vector<VkPipelineStageFlags2> waitStages;
    if (presentedImage) {
        waitSemaphores.push_back(&engine.imageAvailableSemaphores[frame]);
        waitStages.push_back(acquireWaitStage);
        signalSemaphores.push_back(&renderFinishedSemaphores[frame]);
    }
    device.getGraphicsQueue().submit({ &commandBuffer }, fences[frame], waitSemaphores, waitStages, signalSemaphores);

    if (presentedImage) engine.presentImage(*imageIndex, renderFinishedSemaphores[frame]);
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
                ownedComputePipelines.at(*pass.pipelineId).bind(commandBuffer);
                bindDescriptors(commandBuffer, pass);
            }
            PassContext context(*this, commandBuffer, pass, {});
            if (info.execute) info.execute(context);
            break;
        }
        case PassType::Graphics: {
            const std::optional<ImageAccess> attachment = findColorAttachment(pass);
            if (!attachment)
                throw std::runtime_error(std::format("The graphics pass [{}] has no color attachment", pass.name));

            const VkExtent2D extent = getImageExtent(attachment->image.id);
            VkClearColorValue clearColor = {};
            std::memcpy(clearColor.float32, info.clearColor.data(), sizeof(clearColor.float32));
            dynamicRenderer.begin(
                commandBuffer,
                bindings.at(attachment->image.id).view, imageAccessToVk(attachment->info).layout,
                attachmentLoadToVk(attachment->info.attachmentLoad), VK_ATTACHMENT_STORE_OP_STORE,
                clearColor, VkRect2D{ {}, extent }
            );

            if (pass.pipelineId) {
                GraphicsPipeline& pipeline = ownedGraphicsPipelines.at(*pass.pipelineId);
                pipeline.bind(commandBuffer);
                pipeline.setViewport(commandBuffer, VkViewport{ 0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.0f, 1.0f });
                pipeline.setScissor(commandBuffer, VkRect2D{ {}, extent });
                bindDescriptors(commandBuffer, pass);
            }
            PassContext context(*this, commandBuffer, pass, extent);
            if (info.execute) info.execute(context);

            dynamicRenderer.end(commandBuffer);
            break;
        }
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

void RenderGraph::bindDescriptors(const CommandBuffer& commandBuffer, const CompiledPass& pass) {
    uint32_t passId = pass.pass.id;
    auto groupIt = passDescriptorGroups.find(passId);
    if (groupIt == passDescriptorGroups.end()) return;

    auto dirtyIt = dirtyDescriptors.find(passId * MAX_FRAME_IN_FLIGHT + frame);
    if (dirtyIt != dirtyDescriptors.end()) {
        updateDescriptors(pass.pass, frame);
        dirtyDescriptors.erase(dirtyIt);
    }

    VkPipelineLayout layout;
    VkPipelineBindPoint bindPoint;
    if (pass.type == PassType::Compute) {
        layout    = ownedComputePipelines.at(*pass.pipelineId).getLayout();
        bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
    } else {
        layout    = ownedGraphicsPipelines.at(*pass.pipelineId).getLayout();
        bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    }

    for (uint32_t set = 0; set < groupIt->second.size(); ++set)
        if (groupIt->second[set].isValid()) groupIt->second[set].at(frame).bind(commandBuffer, layout, bindPoint, set);
}

std::optional<ImageAccess> RenderGraph::findColorAttachment(const CompiledPass& pass) const {
    for (const ImageAccess& access : pass.imageAccesses)
        if (access.info.usage == ImageUsageType::ColorAttachment) return access;
    return std::nullopt;
}

VkExtent2D RenderGraph::getImageExtent(uint32_t imageId) const {
    if (swapchainImage && imageId == swapchainImage->id) return engine.getExtent();
    const ImageInfo& info = graph.images[imageId];
    return { info.width, info.height };
}

std::string RenderGraph::dump() const {
    return dumpGraph(graph);
}

// ——— Pass context ————————————————————————————————————————————————————————————

VkCommandBuffer PassContext::cmd() const {
    return commandBuffer.get();
}

void PassContext::dispatch(uint32_t x, uint32_t y, uint32_t z) {
    if (pass.type != PassType::Compute || !pass.pipelineId)
        throw std::runtime_error(std::format("The pass [{}] has no compute pipeline to dispatch", pass.name));
    graph.ownedComputePipelines.at(*pass.pipelineId).dispatch(commandBuffer, x, y, z);
}

void PassContext::draw(uint32_t vertexCount, uint32_t instanceCount) {
    if (pass.type != PassType::Graphics || !pass.pipelineId)
        throw std::runtime_error(std::format("The pass [{}] has no graphics pipeline to draw with", pass.name));
    vkCmdDraw(commandBuffer.get(), vertexCount, instanceCount, 0, 0);
}

void PassContext::pushConstants(const void* data, uint32_t size) {
    if (!pass.pipelineId)
        throw std::runtime_error(std::format("The pass [{}] has no pipeline to push constants to", pass.name));

    const bool compute = pass.type == PassType::Compute;
    const uint32_t capacity = compute
        ? graph.computePushConstantSizes.at(*pass.pipelineId)
        : graph.graphicsPushConstantSizes.at(*pass.pipelineId);
    if (size > capacity)
        throw std::runtime_error(std::format("The pass [{}] pushes {} bytes of constants but its shaders declare {}", pass.name, size, capacity));

    const VkPipelineLayout layout = compute
        ? graph.ownedComputePipelines.at(*pass.pipelineId).getLayout()
        : graph.ownedGraphicsPipelines.at(*pass.pipelineId).getLayout();
    const VkShaderStageFlags stages = compute ? VK_SHADER_STAGE_COMPUTE_BIT : (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    vkCmdPushConstants(commandBuffer.get(), layout, stages, 0, size, data);
}

VkImage PassContext::getImage(ImageHandle handle) const {
    return graph.bindings.at(handle.id).image;
}

VkImageView PassContext::getImageView(ImageHandle handle) const {
    return graph.bindings.at(handle.id).view;
}

VkBuffer PassContext::getBuffer(BufferHandle handle) const {
    return graph.bufferBindings.at(handle.id);
}

}
