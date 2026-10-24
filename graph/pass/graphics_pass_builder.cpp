#include "graphics_pass_builder.hpp"

#include "../graph_utils.hpp"

#include <algorithm>
#include <stdexcept>

namespace VkSmol {

GraphicsPassBuilder& GraphicsPassBuilder::pipeline(
    std::string vertexShaderPath, std::string fragmentShaderPath, ShaderDefines shaderDefines, GraphicsState state,
    ShaderConformances shaderConformances
) {
    return pipeline(
        {{VK_SHADER_STAGE_VERTEX_BIT, std::move(vertexShaderPath)},
         {VK_SHADER_STAGE_FRAGMENT_BIT, std::move(fragmentShaderPath)}},
        std::move(shaderDefines), std::move(state), std::move(shaderConformances)
    );
}

GraphicsPassBuilder& GraphicsPassBuilder::pipeline(
    std::vector<std::pair<VkShaderStageFlagBits, std::string>> shaderPaths, ShaderDefines shaderDefines,
    GraphicsState state, ShaderConformances shaderConformances
) {
    GraphicsPipelineInfo info;
    info.name               = graph->passes[handle.id].name + "Pipeline";
    info.shaderPaths        = std::move(shaderPaths);
    info.shaderDefines      = std::move(shaderDefines);
    info.shaderConformances = std::move(shaderConformances);
    info.state              = std::move(state);
    info.passId             = handle.id;
    graph->passes[handle.id].pipelineId = static_cast<uint32_t>(graph->graphicsPipelines.size());
    graph->graphicsPipelines.push_back(std::move(info));
    return *this;
}

GraphicsPassBuilder& GraphicsPassBuilder::vertexInput(const VkPipelineVertexInputStateCreateInfo& vertexInput) {
    for (GraphicsPipelineInfo& info : graph->graphicsPipelines) {
        if (info.passId != handle.id) continue;
        if (vertexInput.pVertexBindingDescriptions)
            info.bindingDescription = *vertexInput.pVertexBindingDescriptions;
        info.attributeDescriptions.assign(vertexInput.pVertexAttributeDescriptions, vertexInput.pVertexAttributeDescriptions + vertexInput.vertexAttributeDescriptionCount);
        return *this;
    }
    throw std::runtime_error("vertexInput() must be called after pipeline()");
}

GraphicsPassBuilder& GraphicsPassBuilder::vertexBuffer(BufferHandle buffer) {
    graph->passes[handle.id].bufferAccesses.push_back({ .buffer = buffer, .info = { .usage = BufferUsageType::Vertex, .access = AccessType::Read } });
    return *this;
}

GraphicsPassBuilder& GraphicsPassBuilder::indexBuffer(BufferHandle buffer, VkIndexType indexType) {
    graph->passes[handle.id].bufferAccesses.push_back({ .buffer = buffer, .info = { .usage = BufferUsageType::Index, .access = AccessType::Read } });
    graph->passes[handle.id].indexType = indexType;
    return *this;
}

GraphicsPassBuilder&
GraphicsPassBuilder::colorAttachment(ImageRange image, AttachmentLoad load, std::array<float, 4> clearColor) {
    const ImageAccessInfo info{
        .usage          = ImageUsageType::ColorAttachment,
        .access         = AccessType::Write,
        .writeMode      = load == AttachmentLoad::Load ? WriteMode::Preserve : WriteMode::Overwrite,
        .attachmentLoad = load,
    };
    checkImageAccessInfoValid(info);
    VkClearValue clearValue = {};
    std::copy(clearColor.begin(), clearColor.end(), clearValue.color.float32);
    graph->passes[handle.id].imageAccesses.push_back(
        {.image = image.image, .subresources = image.subresources, .info = info, .clearValue = clearValue}
    );
    return *this;
}

GraphicsPassBuilder& GraphicsPassBuilder::depthAttachment(ImageRange image, AttachmentLoad load, float clearDepth) {
    const ImageAccessInfo info{
        .usage = ImageUsageType::DepthAttachment,
        .access = AccessType::Write,
        .writeMode = load == AttachmentLoad::Load ? WriteMode::Preserve : WriteMode::Overwrite,
        .attachmentLoad = load,
    };
    checkImageAccessInfoValid(info);
    graph->passes[handle.id].imageAccesses.push_back({
        .image = image.image,
        .subresources = image.subresources,
        .info = info,
        .clearValue = {.depthStencil = {.depth = clearDepth, .stencil = 0}},
    });
    return *this;
}

GraphicsPassBuilder& GraphicsPassBuilder::readOnlyDepthAttachment(ImageRange image) {
    return access(image, ImageUsageType::DepthAttachment, AccessType::Read);
}
}
