#include "compute_pass_builder.hpp"

#include "../render_graph_builder.hpp"

namespace VkSmol {

ComputePassBuilder&
ComputePassBuilder::pipeline(std::string shaderPath, ShaderDefines shaderDefines, ShaderConformances shaderConformances) {
    graph->computePipelines.push_back({
        .name               = graph->passes[handle.id].name + "Pipeline",
        .shaderPath         = std::move(shaderPath),
        .shaderDefines      = std::move(shaderDefines),
        .shaderConformances = std::move(shaderConformances),
        .passId             = handle.id,
    });
    return *this;
}

}
