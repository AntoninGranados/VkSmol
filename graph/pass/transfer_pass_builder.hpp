#pragma once

#include "pass_builder.hpp"
#include "../builder_resource.hpp"

namespace VkSmol {

class TransferPassBuilder: public PassBuilder<TransferPassBuilder> {
public:
    TransferPassBuilder& copyFrom(ImageRange image);
    TransferPassBuilder& copyTo(ImageRange image);

private:
    TransferPassBuilder(PassHandle handle, RenderGraphBuilder* graph): PassBuilder(handle, graph) {}

    friend class RenderGraphBuilder;
};

}
