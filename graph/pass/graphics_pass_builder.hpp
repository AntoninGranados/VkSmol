#pragma once

#include "../builder_resource.hpp"
#include "pass_builder.hpp"

class RenderGraphBuilder;

class GraphicsPassBuilder: public PassBuilder {
public:
    void readImage  (uint32_t binding, ImageHandle  imageHandle, ImageUsageType  usage);
    void writeImage (ImageHandle imageHandle, ImageUsageType usage,
                     WriteMode writeMode = WriteMode::Overwrite,
                     AttachmentLoad attachmentLoad = AttachmentLoad::DontCare);
    void readBuffer (uint32_t binding, BufferHandle buffer,      BufferUsageType usage);
    void writeBuffer(uint32_t binding, BufferHandle buffer,      BufferUsageType usage);
    void bindVertexBuffer(BufferHandle buffer);
    void bindIndexBuffer (BufferHandle buffer);

    GraphicsPipelineHandle setPipeline(VkPipelineVertexInputStateCreateInfo vi, std::vector<std::pair<VkShaderStageFlagBits, std::string>> shaders);

private:
    GraphicsPassBuilder(PassHandle handle, RenderGraphBuilder* graph)
        : PassBuilder(handle, graph) {}

    friend class RenderGraphBuilder;
};
