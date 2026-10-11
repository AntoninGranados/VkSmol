#pragma once

#include "../builder_resource.hpp"
#include "pass_builder.hpp"

class RenderGraphBuilder;

class ComputePassBuilder: public PassBuilder {
public:
    void readImage  (ShaderLocation location, ImageHandle  imageHandle, ImageUsageType  usage);
    void writeImage (ShaderLocation location, ImageHandle  imageHandle, ImageUsageType  usage);
    void readBuffer (ShaderLocation location, BufferHandle buffer,      BufferUsageType usage);
    void writeBuffer(ShaderLocation location, BufferHandle buffer,      BufferUsageType usage);

    ComputePipelineHandle setPipeline(std::string shaderPath, ShaderDefines shaderDefines = {});

private:
    ComputePassBuilder(PassHandle handle, RenderGraphBuilder* graph)
        : PassBuilder(handle, graph) {}

    friend class RenderGraphBuilder;
};
