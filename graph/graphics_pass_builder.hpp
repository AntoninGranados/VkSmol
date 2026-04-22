#pragma once

#include "builder_resource.hpp"
#include "pass_builder.hpp"

class RenderGraphBuilder;

class GraphicsPassBuilder: public PassBuilder {
public:
    void readImage(ImageHandle imageHandle, ImageUsage usage);
    void writeImage(ImageHandle imageHandle, ImageUsage usage);

private:
      GraphicsPassBuilder(PassHandle handle, RenderGraphBuilder* graph)
        : PassBuilder(handle, graph) {}

      friend class RenderGraphBuilder;
};
