#pragma once

#include "pass_builder.hpp"
#include "../builder_resource.hpp"

namespace VkSmol {

class TransferPassBuilder: public PassBuilder<TransferPassBuilder> {
public:
    TransferPassBuilder& copyFrom(ImageHandle imageHandle);
    TransferPassBuilder& copyTo(ImageHandle imageHandle);

private:
    TransferPassBuilder(PassHandle handle, RenderGraphBuilder* graph): PassBuilder(handle, graph) {}

    friend class RenderGraphBuilder;
};

}
