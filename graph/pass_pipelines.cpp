#include "pass_pipelines.hpp"

#include <algorithm>
#include <format>
#include <optional>
#include <print>
#include <stdexcept>
#include <utility>

#include "../command/command_buffer.hpp"
#include "../device/device.hpp"
#include "../resource/resource_registry.hpp"
#include "../utils.hpp"
#include "graph_utils.hpp"
#include "pass_descriptors.hpp"

namespace VkSmol {

PassPipelines::PassPipelines(Device& device, ResourceRegistry& resources)
    : device(device), logicalDevice(device.getLogicalDevice()), resources(resources) {}

void PassPipelines::init(
    const CompiledRenderGraph& graph, const PassDescriptors& descriptors, const std::string& cachePath
) {
    cache.init(logicalDevice, cachePath);
    try {
        createComputePipelines(graph, descriptors);
        createGraphicsPipelines(graph, descriptors);
    } catch (...) {
        device.markShadersStale();
        destroy();
        throw;
    }
    device.markShadersStale();
}

void PassPipelines::destroy() {
    for (auto& [id, compute] : computes) compute.pipeline.destroy(logicalDevice);
    computes.clear();
    for (auto& [id, graphic] : graphics) graphic.pipeline.destroy(logicalDevice);
    graphics.clear();
    cache.destroy(logicalDevice);
}

void PassPipelines::bind(const CommandBuffer& commandBuffer, const CompiledPass& pass) {
    if (pass.type == PassType::Compute)
        computes.at(*pass.pipelineId).pipeline.bind(commandBuffer);
    else
        graphics.at(*pass.pipelineId).pipeline.bind(commandBuffer);
}

VkPipelineLayout PassPipelines::getLayout(const CompiledPass& pass) {
    if (pass.type == PassType::Compute) return computes.at(*pass.pipelineId).pipeline.getLayout();
    return graphics.at(*pass.pipelineId).pipeline.getLayout();
}

void PassPipelines::setRenderArea(const CommandBuffer& commandBuffer, const CompiledPass& pass, VkExtent2D extent) {
    GraphicsPipeline& pipeline = graphics.at(*pass.pipelineId).pipeline;
    pipeline.setViewport(
        commandBuffer,
        VkViewport{0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.0f, 1.0f}
    );
    pipeline.setScissor(commandBuffer, VkRect2D{{}, extent});
}

void PassPipelines::dispatch(
    const CommandBuffer& commandBuffer, const CompiledPass& pass, uint32_t x, uint32_t y, uint32_t z
) {
    computes.at(*pass.pipelineId).pipeline.dispatch(commandBuffer, x, y, z);
}

VkExtent3D PassPipelines::getThreadGroupSize(const CompiledPass& pass) const {
    return computes.at(*pass.pipelineId).threadGroupSize;
}

void PassPipelines::pushConstants(
    const CommandBuffer& commandBuffer, const CompiledPass& pass, const void* data, uint32_t size
) {
    if (!pass.pipelineId)
        throw std::runtime_error(std::format("The pass [{}] has no pipeline to push constants to", pass.name));

    const bool compute = pass.type == PassType::Compute;
    const uint32_t capacity =
        compute ? computes.at(*pass.pipelineId).pushConstantSize : graphics.at(*pass.pipelineId).pushConstantSize;
    if (size > capacity)
        throw std::runtime_error(
            std::format(
                "The pass [{}] pushes {} bytes of constants but its shaders declare {}", pass.name, size, capacity
            )
        );

    const VkShaderStageFlags stages =
        compute ? VK_SHADER_STAGE_COMPUTE_BIT : graphics.at(*pass.pipelineId).pushConstantStages;
    vkCmdPushConstants(commandBuffer.get(), getLayout(pass), stages, 0, size, data);
}

void PassPipelines::createComputePipelines(const CompiledRenderGraph& graph, const PassDescriptors& descriptors) {
    for (uint32_t id = 0; id < graph.computePipelines.size(); ++id) {
        const ComputePipelineInfo& info = graph.computePipelines[id];
        const CompiledPass& pass = graph.compiledPasses[info.passId];
        const ShaderCompilation& compilation = compileProgram(
            pass, {{VK_SHADER_STAGE_COMPUTE_BIT, info.shaderPath}}, info.shaderDefines, info.shaderConformances
        );
        Shader shader;
        shader.init(logicalDevice, VK_SHADER_STAGE_COMPUTE_BIT, compilation.stages.front().spirv);

        Compute compute{
            .pushConstantSize = compilation.pushConstantSize, .threadGroupSize = compilation.threadGroupSize
        };
        compute.pipeline.init(
            logicalDevice, shader, descriptors.getLayouts(info.passId), compilation.pushConstantSize, cache
        );
        computes[id] = std::move(compute);
        shader.destroy(logicalDevice);
    }
}

void PassPipelines::createGraphicsPipelines(const CompiledRenderGraph& graph, const PassDescriptors& descriptors) {
    for (uint32_t id = 0; id < graph.graphicsPipelines.size(); ++id) {
        const GraphicsPipelineInfo& info = graph.graphicsPipelines[id];
        const CompiledPass& pass = graph.compiledPasses[info.passId];
        const bool hasVertexInput = !info.attributeDescriptions.empty();
        const VkPipelineVertexInputStateCreateInfo vertexInput{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = hasVertexInput ? 1u : 0u,
            .pVertexBindingDescriptions = hasVertexInput ? &info.bindingDescription : nullptr,
            .vertexAttributeDescriptionCount = static_cast<uint32_t>(info.attributeDescriptions.size()),
            .pVertexAttributeDescriptions = info.attributeDescriptions.data(),
        };

        const ShaderCompilation& compilation =
            compileProgram(pass, info.shaderPaths, info.shaderDefines, info.shaderConformances);
        std::vector<Shader> shaders(compilation.stages.size());
        std::vector<const Shader*> shaderPointers;
        Graphics graphic{.pushConstantSize = compilation.pushConstantSize};
        for (size_t i = 0; i < compilation.stages.size(); ++i) {
            const ShaderStageCompilation& stage = compilation.stages[i];
            graphic.pushConstantStages |= stage.stage;
            shaders[i].init(logicalDevice, stage.stage, stage.spirv);
            shaderPointers.push_back(&shaders[i]);
        }

        std::vector<VkFormat> colorFormats;
        for (const ImageAccess& access : findColorAttachments(pass.imageAccesses))
            colorFormats.push_back(resources.getImage(access.image).format);
        const std::optional<ImageAccess> depth = findDepthAttachment(pass.imageAccesses);
        const VkPipelineRenderingCreateInfo rendering{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .colorAttachmentCount = static_cast<uint32_t>(colorFormats.size()),
            .pColorAttachmentFormats = colorFormats.data(),
            .depthAttachmentFormat = depth ? resources.getImage(depth->image).format : VK_FORMAT_UNDEFINED,
        };

        GraphicsState state = info.state;
        if (!state.depth && depth) state.depth = DepthState{.write = depth->info.access == AccessType::Write};
        if (state.blend.empty()) state.blend.assign(colorFormats.size(), BlendState::opaque());

        graphic.pipeline.init(
            logicalDevice, rendering, vertexInput, shaderPointers, descriptors.getLayouts(info.passId),
            graphic.pushConstantSize, graphic.pushConstantStages, state, cache
        );
        graphics[id] = std::move(graphic);
        for (Shader& shader : shaders) shader.destroy(logicalDevice);
    }
}

const ShaderCompilation& PassPipelines::compileProgram(
    const CompiledPass& pass, const std::vector<std::pair<VkShaderStageFlagBits, std::string>>& stages,
    const ShaderDefines& defines, const ShaderConformances& conformances
) {
    std::vector<std::string> paths;
    for (const auto& [stage, path] : stages) paths.push_back(path);
    const Device::CompiledShader& compiled = device.compileShader(paths, defines, conformances);
    if (!compiled.error.empty()) std::println(stderr, "{}", compiled.error);

    for (size_t i = 0; i < stages.size(); ++i)
        if (compiled.compilation.stages[i].stage != stages[i].first)
            throw std::runtime_error(
                std::format(
                    "The shader [{}{}{}] does not have the stage expected by the pass [{}]", MSG_RED_B, paths[i],
                    MSG_RESET, pass.name
                )
            );

    for (const ShaderParameter& parameter : compiled.compilation.parameters) {
        if (!parameter.used) continue;
        const bool bound = std::ranges::any_of(pass.descriptorBindings, [&](const DescriptorBindingDecl& decl) {
            return decl.set == parameter.location.set && decl.binding == parameter.location.binding;
        });
        if (!bound)
            throw std::runtime_error(
                std::format(
                    "The pass [{}] does not bind the parameter [{}{}{}] (set {}, binding {}) used by its shaders",
                    pass.name, MSG_RED_B, parameter.name, MSG_RESET, parameter.location.set, parameter.location.binding
                )
            );
    }
    return compiled.compilation;
}

} // namespace VkSmol
