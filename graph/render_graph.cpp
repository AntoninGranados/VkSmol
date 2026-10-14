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

namespace VkSmol {

RenderGraph::RenderGraph(Engine& engine)
    : engine(engine), device(engine.getDevice()), logicalDevice(engine.getDevice().getLogicalDevice()) {
    dynamicRenderer.init(logicalDevice);
}

RenderGraph::~RenderGraph() {
    device.waitIdle();
    for (const Fence& fence : fences) engine.unregisterFence(fence);
    if (timestampPool != VK_NULL_HANDLE)
        vkDestroyQueryPool(logicalDevice.get(), timestampPool, nullptr);
    if (baked) {
        destroySync();
        destroyPipelines();
        pipelineCache.destroy(logicalDevice);
    }
    destroyDescriptorSets();
    for (ImageHandle image : ownedImages) engine.destroyImage(image);
    for (BufferHandle buffer : ownedBuffers) engine.destroyBuffer(buffer);
}

// ——— Declaration —————————————————————————————————————————————————————————————

ImageHandle RenderGraph::createImage(std::string name, VkFormat format, uint32_t width, uint32_t height) {
    assert(!baked);
    return ownedImages.emplace_back(engine.createImage(std::move(name), format, width, height));
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

std::vector<std::string> RenderGraph::bake(std::string pipelineCachePath) {
    assert(!baked);
    std::vector<std::string> errors;

    resolveBindings(errors);
    if (!errors.empty()) return errors;
    graph = builder.compile();

    if (presentedImage) {
        for (const CompiledPass& pass : graph.compiledPasses) {
            const auto access = std::ranges::find_if(pass.imageAccesses, [&](const ImageAccess& a) { return a.image.id == presentedImage->id; });
            if (access == pass.imageAccesses.end()) continue;
            acquireWaitStage = imageAccessToVk(access->info).stage;
            break;
        }
    }

    pipelineCache.init(logicalDevice, pipelineCachePath);
    initDescriptorSets();
    try {
        std::vector<std::string> pipelineErrors = initPipelines();
        errors.insert(errors.end(), pipelineErrors.begin(), pipelineErrors.end());
    } catch (const std::exception& e) {
        errors.push_back(e.what());
    }
    device.markShadersStale();
    initSync();
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
    writtenDescriptors.clear();
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
                set.linkUniformBuffer(payload.buffer, payload.size, write.binding);
            } else if constexpr (std::is_same_v<T, StorageBufferPayload>) {
                assert(payload.buffer);
                set.linkStorageBuffer(payload.buffer, payload.size, write.binding);
            } else if constexpr (std::is_same_v<T, CombinedImageSamplerPayload>) {
                assert(payload.view && payload.sampler);
                set.linkCombinedImageSampler(payload.view, payload.sampler, write.binding);
            } else if constexpr (std::is_same_v<T, StorageImagePayload>) {
                assert(payload.view);
                set.linkStorageImage(payload.view, write.binding);
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

std::vector<uint64_t> RenderGraph::resolveDescriptors(const CompiledPass& pass) const {
    std::vector<uint64_t> resolved;
    for (const DescriptorBindingDecl& decl : pass.descriptorBindings) {
        if (decl.isImage) {
            const ImageHandle image = { .id = decl.handleId };
            resolved.push_back(reinterpret_cast<uint64_t>(engine.getImageView(image)));
            resolved.push_back(reinterpret_cast<uint64_t>(engine.getSampler(image)));
        } else {
            const BufferHandle buffer = { .id = decl.handleId };
            resolved.push_back(reinterpret_cast<uint64_t>(engine.getBuffer(buffer)));
            resolved.push_back(engine.getBufferSize(buffer));
        }
    }
    return resolved;
}

void RenderGraph::updateDescriptors(const CompiledPass& pass, uint32_t frameIndex) {
    auto groupIt = passDescriptorGroups.find(pass.pass.id);
    if (groupIt == passDescriptorGroups.end()) return;

    const std::vector<DescriptorSetLayout>& layouts = passDescriptorLayouts.at(pass.pass.id);
    for (uint32_t set = 0; set < layouts.size(); ++set) {
        if (!groupIt->second[set].isValid()) continue;

        DescriptorWriter writer(layouts[set]);
        for (const DescriptorBindingDecl& decl : pass.descriptorBindings) {
            if (decl.set != set) continue;
            if (decl.isImage) {
                const ImageHandle image = { .id = decl.handleId };
                if (decl.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                    writer.combinedImageSampler(decl.binding, engine.getImageView(image), engine.getSampler(image));
                else if (decl.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    writer.storageImage(decl.binding, engine.getImageView(image));
            } else {
                const BufferHandle buffer = { .id = decl.handleId };
                if (decl.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                    writer.uniform(decl.binding, engine.getBuffer(buffer), engine.getBufferSize(buffer));
                else if (decl.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
                    writer.storage(decl.binding, engine.getBuffer(buffer), engine.getBufferSize(buffer));
            }
        }
        updateDescriptorSetGroup(groupIt->second[set], frameIndex, writer);
    }
}

void RenderGraph::bindDescriptors(const CommandBuffer& commandBuffer, const CompiledPass& pass) {
    const uint32_t passId = pass.pass.id;
    auto groupIt = passDescriptorGroups.find(passId);
    if (groupIt == passDescriptorGroups.end()) return;

    std::vector<uint64_t> resolved = resolveDescriptors(pass);
    std::vector<uint64_t>& written = writtenDescriptors[passId * MAX_FRAME_IN_FLIGHT + frame];
    if (written != resolved) {
        updateDescriptors(pass, frame);
        written = std::move(resolved);
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

        const bool hasVertexInput = !info.attributeDescriptions.empty();
        const VkPipelineVertexInputStateCreateInfo vertexInput{
            .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount   = hasVertexInput ? 1u : 0u,
            .pVertexBindingDescriptions      = hasVertexInput ? &info.bindingDescription : nullptr,
            .vertexAttributeDescriptionCount = static_cast<uint32_t>(info.attributeDescriptions.size()),
            .pVertexAttributeDescriptions    = info.attributeDescriptions.data(),
        };

        std::vector<Shader> shaders(info.shaderPaths.size());
        std::vector<const Shader*> shaderPointers;
        uint32_t pushConstantSize = 0;
        VkShaderStageFlags pushConstantStages = 0;
        for (size_t i = 0; i < info.shaderPaths.size(); ++i) {
            const auto& [stage, path] = info.shaderPaths[i];
            pushConstantStages |= stage;
            shaders[i].init(logicalDevice, stage, compilePassShader(pass, stage, path, info.shaderDefines, errors));
            shaderPointers.push_back(&shaders[i]);
            pushConstantSize = std::max(pushConstantSize, device.compileShader(path, info.shaderDefines).compilation.pushConstantSize);
        }

        const VkFormat format = engine.getImageFormat(attachment->image);
        VkPipelineRenderingCreateInfoKHR rendering{};
        rendering.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        rendering.colorAttachmentCount    = 1;
        rendering.pColorAttachmentFormats = &format;

        GraphicsPipeline pipeline;
        pipeline.init(logicalDevice, rendering, vertexInput, shaderPointers, layoutsOf(info.passId), pushConstantSize, pushConstantStages, pipelineCache);
        ownedGraphicsPipelines[id] = std::move(pipeline);
        graphicsPushConstantSizes[id] = pushConstantSize;
        graphicsPushConstantStages[id] = pushConstantStages;
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

// ——— Barriers ————————————————————————————————————————————————————————————————

void RenderGraph::emitBarriers(const CommandBuffer& commandBuffer, const CompiledPass& pass) {
    for (const ImageAccess& access : pass.imageAccesses) {
        Engine::PersistentImage& image = engine.images[access.image.id];
        const ImageAccessInfo& current = image.lastAccess;
        if (current.usage == access.info.usage && current.access != AccessType::Write && access.info.access != AccessType::Write)
            continue;
        engine.transitionImage(commandBuffer, image, access.info);
    }

    for (const BufferAccess& access : pass.bufferAccesses) {
        Engine::PersistentBuffer& buffer = engine.buffers[access.buffer.id];
        const std::optional<BufferAccessInfo>& current = buffer.lastAccess;
        if (!current || (current->access == AccessType::Read && access.info.access == AccessType::Read)) {
            buffer.lastAccess = access.info;
            continue;
        }
        engine.transitionBuffer(commandBuffer, buffer, access.info);
    }
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
    engine.flushWrites(commandBuffer);
    for (const CompiledPass& pass : graph.compiledPasses)
        recordPass(commandBuffer, pass);
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
    engine.registerFence(frame, fences[frame]);

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

            const VkExtent2D extent = engine.getImageExtent(attachment->image);
            VkClearColorValue clearColor = {};
            std::memcpy(clearColor.float32, info.clearColor.data(), sizeof(clearColor.float32));
            dynamicRenderer.begin(
                commandBuffer,
                engine.getImageView(attachment->image), imageAccessToVk(attachment->info).layout,
                attachmentLoadToVk(attachment->info.attachmentLoad), VK_ATTACHMENT_STORE_OP_STORE,
                clearColor, VkRect2D{ {}, extent }
            );

            if (pass.pipelineId) {
                GraphicsPipeline& pipeline = ownedGraphicsPipelines.at(*pass.pipelineId);
                pipeline.bind(commandBuffer);
                pipeline.setViewport(commandBuffer, VkViewport{ 0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.0f, 1.0f });
                pipeline.setScissor(commandBuffer, VkRect2D{ {}, extent });
                bindDescriptors(commandBuffer, pass);

                std::vector<VkBuffer> vertexBuffers;
                for (const BufferAccess& access : pass.bufferAccesses) {
                    if (access.info.usage == BufferUsageType::Vertex) vertexBuffers.push_back(engine.getBuffer(access.buffer));
                    if (access.info.usage == BufferUsageType::Index) vkCmdBindIndexBuffer(commandBuffer.get(), engine.getBuffer(access.buffer), 0, info.indexType);
                }
                if (!vertexBuffers.empty()) {
                    const std::vector<VkDeviceSize> offsets(vertexBuffers.size(), 0);
                    vkCmdBindVertexBuffers(commandBuffer.get(), 0, static_cast<uint32_t>(vertexBuffers.size()), vertexBuffers.data(), offsets.data());
                }
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

std::optional<ImageAccess> RenderGraph::findColorAttachment(const CompiledPass& pass) const {
    for (const ImageAccess& access : pass.imageAccesses)
        if (access.info.usage == ImageUsageType::ColorAttachment) return access;
    return std::nullopt;
}

std::string RenderGraph::dump() const {
    return dumpGraph(graph, engine);
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

void PassContext::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
    if (pass.type != PassType::Graphics || !pass.pipelineId)
        throw std::runtime_error(std::format("The pass [{}] has no graphics pipeline to draw with", pass.name));
    vkCmdDraw(commandBuffer.get(), vertexCount, instanceCount, firstVertex, firstInstance);
}

void PassContext::drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
    if (pass.type != PassType::Graphics || !pass.pipelineId)
        throw std::runtime_error(std::format("The pass [{}] has no graphics pipeline to draw with", pass.name));
    vkCmdDrawIndexed(commandBuffer.get(), indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
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
    const VkShaderStageFlags stages = compute ? VK_SHADER_STAGE_COMPUTE_BIT : graph.graphicsPushConstantStages.at(*pass.pipelineId);
    vkCmdPushConstants(commandBuffer.get(), layout, stages, 0, size, data);
}

VkImage PassContext::getImage(ImageHandle handle) const {
    return graph.engine.getImage(handle);
}

VkImageView PassContext::getImageView(ImageHandle handle) const {
    return graph.engine.getImageView(handle);
}

VkBuffer PassContext::getBuffer(BufferHandle handle) const {
    return graph.engine.getBuffer(handle);
}

}
