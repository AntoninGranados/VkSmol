#pragma once

#include "pass_builder.hpp"
#include "../builder_resource.hpp"

class TransferPassBuilder: public PassBuilder {
public:
    void copyFrom(ImageHandle imageHandle);
    void copyTo(ImageHandle imageHandle);

private:
    TransferPassBuilder(PassHandle handle, RenderGraphBuilder* graph): PassBuilder(handle, graph) {}

    friend class RenderGraphBuilder;
};
