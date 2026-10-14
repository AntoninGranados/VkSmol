#pragma once

#include <array>

#include "../builder_resource.hpp"
#include "pass_builder.hpp"

namespace VkSmol {

class RenderGraphBuilder;

class GraphicsPassBuilder: public PassBuilder<GraphicsPassBuilder> {
public:
    GraphicsPassBuilder& pipeline(std::string vertexShaderPath, std::string fragmentShaderPath, ShaderDefines shaderDefines = {});
    GraphicsPassBuilder& colorAttachment(ImageHandle image, AttachmentLoad load, std::array<float, 4> clearColor = { 0.0f, 0.0f, 0.0f, 1.0f });

private:
    GraphicsPassBuilder(PassHandle handle, RenderGraphBuilder* graph)
        : PassBuilder(handle, graph) {}

    friend class RenderGraphBuilder;
};

}
