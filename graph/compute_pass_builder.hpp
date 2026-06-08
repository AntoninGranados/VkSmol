#pragma once

#include "builder_resource.hpp"
#include "pass_builder.hpp"

class RenderGraphBuilder;

class ComputePassBuilder: public PassBuilder {
public:
    void readImage(ImageHandle imageHandle, ImageUsageType usage);
    void writeImage(ImageHandle imageHandle, ImageUsageType usage);

private:
    ComputePassBuilder(PassHandle handle, RenderGraphBuilder* graph)
        : PassBuilder(handle, graph) {}

    friend class RenderGraphBuilder;
};
