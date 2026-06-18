#pragma once

#include "../builder_resource.hpp"
#include "pass_builder.hpp"

class RenderGraphBuilder;

class ComputePassBuilder: public PassBuilder {
public:
    void readImage  (uint32_t binding, ImageHandle  imageHandle, ImageUsageType  usage);
    void writeImage (uint32_t binding, ImageHandle  imageHandle, ImageUsageType  usage);
    void readBuffer (uint32_t binding, BufferHandle buffer,      BufferUsageType usage);
    void writeBuffer(uint32_t binding, BufferHandle buffer,      BufferUsageType usage);

    ComputePipelineHandle setPipeline(std::string shaderPath);

private:
    ComputePassBuilder(PassHandle handle, RenderGraphBuilder* graph)
        : PassBuilder(handle, graph) {}

    friend class RenderGraphBuilder;
};
