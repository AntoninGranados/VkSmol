#pragma once

#include "../builder_resource.hpp"
#include "pass_builder.hpp"

namespace VkSmol {

class RenderGraphBuilder;

class ComputePassBuilder: public PassBuilder<ComputePassBuilder> {
public:
    ComputePassBuilder&
    pipeline(std::string shaderPath, ShaderDefines shaderDefines = {}, ShaderConformances shaderConformances = {});

private:
    ComputePassBuilder(PassHandle handle, RenderGraphBuilder* graph)
        : PassBuilder(handle, graph) {}

    friend class RenderGraphBuilder;
};

}
