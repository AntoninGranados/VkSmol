#pragma once

#include "builder_resource.hpp"

class RenderGraphBuilder;

class PassBuilder {
protected:
    PassHandle handle;
    RenderGraphBuilder* graph = nullptr;

    PassBuilder(PassHandle handle, RenderGraphBuilder* graph)
        : handle(handle), graph(graph) {};
    friend RenderGraphBuilder;
};
