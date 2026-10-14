#include "graphics_pass_builder.hpp"

#include "../graph_utils.hpp"
#include "../render_graph_builder.hpp"

namespace VkSmol {

GraphicsPassBuilder& GraphicsPassBuilder::pipeline(std::string vertexShaderPath, std::string fragmentShaderPath, ShaderDefines shaderDefines) {
    GraphicsPipelineInfo info;
    info.name          = graph->passes[handle.id].name + "Pipeline";
    info.shaderPaths   = { { VK_SHADER_STAGE_VERTEX_BIT, std::move(vertexShaderPath) }, { VK_SHADER_STAGE_FRAGMENT_BIT, std::move(fragmentShaderPath) } };
    info.shaderDefines = std::move(shaderDefines);
    info.passId        = handle.id;
    graph->graphicsPipelines.push_back(std::move(info));
    return *this;
}

GraphicsPassBuilder& GraphicsPassBuilder::colorAttachment(ImageHandle image, AttachmentLoad load, std::array<float, 4> clearColor) {
    const ImageAccessInfo info{
        .usage          = ImageUsageType::ColorAttachment,
        .access         = AccessType::Write,
        .writeMode      = load == AttachmentLoad::Load ? WriteMode::Preserve : WriteMode::Overwrite,
        .attachmentLoad = load,
    };
    checkImageAccessInfoValid(info);
    graph->passes[handle.id].imageAccesses.push_back({ .image = image, .info = info });
    graph->passes[handle.id].clearColor = clearColor;
    return *this;
}

}
