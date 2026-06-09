#pragma once

#include "pass_builder.hpp"

class PresentPassBuilder: public PassBuilder {
public:
    void setPresentationImage(ImageHandle imageHandle);

private:
      PresentPassBuilder(PassHandle handle, RenderGraphBuilder* graph)
        : PassBuilder(handle, graph) {}

      friend class RenderGraphBuilder;
};
