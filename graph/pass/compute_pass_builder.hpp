#pragma once

#include "../builder_resource.hpp"
#include "pass_builder.hpp"

class RenderGraphBuilder;

class ComputePassBuilder: public PassBuilder {
public:
    void readImage(ImageHandle imageHandle, ImageUsageType usage);
    void writeImage(ImageHandle imageHandle, ImageUsageType usage);
    void readBuffer(BufferHandle buffer, BufferUsageType usage);
    void writeBuffer(BufferHandle buffer, BufferUsageType usage);

    // Overloads with descriptor binding slot
    void readImage (ImageHandle  imageHandle, ImageUsageType  usage, uint32_t binding);
    void writeImage(ImageHandle  imageHandle, ImageUsageType  usage, uint32_t binding);
    void readBuffer(BufferHandle buffer,      BufferUsageType usage, uint32_t binding);

    ComputePipelineHandle setPipeline(std::string shaderPath);

private:
    ComputePassBuilder(PassHandle handle, RenderGraphBuilder* graph)
        : PassBuilder(handle, graph) {}

    friend class RenderGraphBuilder;
};
