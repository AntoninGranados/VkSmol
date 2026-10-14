#pragma once

#include "../builder_resource.hpp"
#include "pass_builder.hpp"

namespace VkSmol {

class RenderGraphBuilder;

class GraphicsPassBuilder: public PassBuilder {
public:
    void readImage  (ShaderLocation location, ImageHandle  imageHandle, ImageUsageType  usage);
    void writeImage (ImageHandle imageHandle, ImageUsageType usage,
                     WriteMode writeMode = WriteMode::Overwrite,
                     AttachmentLoad attachmentLoad = AttachmentLoad::DontCare);
    void readBuffer (ShaderLocation location, BufferHandle buffer,      BufferUsageType usage);
    void writeBuffer(ShaderLocation location, BufferHandle buffer,      BufferUsageType usage);
    void bindVertexBuffer(BufferHandle buffer);
    void bindIndexBuffer (BufferHandle buffer);

    GraphicsPipelineHandle setPipeline(VkPipelineVertexInputStateCreateInfo vi, std::vector<std::pair<VkShaderStageFlagBits, std::string>> shaderPaths, ShaderDefines shaderDefines = {});

private:
    GraphicsPassBuilder(PassHandle handle, RenderGraphBuilder* graph)
        : PassBuilder(handle, graph) {}

    friend class RenderGraphBuilder;
};

}
