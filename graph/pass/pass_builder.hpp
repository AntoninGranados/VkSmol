#pragma once

#include "../builder_resource.hpp"

namespace VkSmol {

class RenderGraphBuilder;

class PassBuilder {
public:
    const PassHandle getHandle() { return handle; }
    void setGroup(SubmissionGroupHandle group);
    TimestampHandle setTimestamp();

protected:
    PassHandle handle;
    RenderGraphBuilder* graph = nullptr;

    PassBuilder(PassHandle handle, RenderGraphBuilder* graph)
        : handle(handle), graph(graph) {};
    friend RenderGraphBuilder;
};

}
