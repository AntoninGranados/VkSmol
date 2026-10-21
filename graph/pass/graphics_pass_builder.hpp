#pragma once

#include <array>
#include <utility>
#include <vector>

#include "../builder_resource.hpp"
#include "pass_builder.hpp"

namespace VkSmol {

class RenderGraphBuilder;

class GraphicsPassBuilder: public PassBuilder<GraphicsPassBuilder> {
public:
    GraphicsPassBuilder& pipeline(
        std::string vertexShaderPath, std::string fragmentShaderPath, ShaderDefines shaderDefines = {},
        GraphicsState state = {}, ShaderConformances shaderConformances = {}
    );
    GraphicsPassBuilder& pipeline(
        std::vector<std::pair<VkShaderStageFlagBits, std::string>> shaderPaths, ShaderDefines shaderDefines = {},
        GraphicsState state = {}, ShaderConformances shaderConformances = {}
    );
    GraphicsPassBuilder& vertexInput(const VkPipelineVertexInputStateCreateInfo& vertexInput);
    GraphicsPassBuilder& vertexBuffer(BufferHandle buffer);
    GraphicsPassBuilder& indexBuffer(BufferHandle buffer, VkIndexType indexType = VK_INDEX_TYPE_UINT32);
    GraphicsPassBuilder&
    colorAttachment(ImageRange image, AttachmentLoad load, std::array<float, 4> clearColor = {0.0f, 0.0f, 0.0f, 1.0f});
    GraphicsPassBuilder& depthAttachment(ImageRange image, AttachmentLoad load, float clearDepth = 1.0f);
    GraphicsPassBuilder& readOnlyDepthAttachment(ImageRange image);

private:
    GraphicsPassBuilder(PassHandle handle, RenderGraphBuilder* graph)
        : PassBuilder(handle, graph) {}

    friend class RenderGraphBuilder;
};

}
