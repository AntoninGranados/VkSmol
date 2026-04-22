#pragma once

#include "pass_builder.hpp"

class TransferPassBuilder: public PassBuilder {
private:
      TransferPassBuilder(PassHandle handle, RenderGraphBuilder* graph)
        : PassBuilder(handle, graph) {}

      friend class RenderGraphBuilder;
};
