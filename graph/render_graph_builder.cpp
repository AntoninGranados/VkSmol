#include "render_graph_builder.hpp"

#include "builder_resource.hpp"
#include "compiled_resource.hpp"
#include "pass/graphics_pass_builder.hpp"
#include "pass/compute_pass_builder.hpp"
#include "pass/transfer_pass_builder.hpp"

namespace VkSmol {

GraphicsPassBuilder RenderGraphBuilder::addGraphicsPass(std::string name) {
    return addPass<GraphicsPassBuilder, PassType::Graphics>(name);
}

ComputePassBuilder RenderGraphBuilder::addComputePass(std::string name) {
    return addPass<ComputePassBuilder, PassType::Compute>(name);
}

TransferPassBuilder RenderGraphBuilder::addTransferPass(std::string name) {
    return addPass<TransferPassBuilder, PassType::Transfer>(name);
}

PassHandle RenderGraphBuilder::addPresentPass(ImageHandle image) {
    PassHandle handle = { .id = static_cast<uint32_t>(passes.size()) };
    passes.push_back({
        .name = "Present",
        .type = PassType::Present,
        .imageAccesses = { { .image = image, .info = { .usage = ImageUsageType::Present, .access = AccessType::Read } } },
    });
    return handle;
}

CompiledRenderGraph RenderGraphBuilder::compile() const {
    CompiledRenderGraph compiled;
    compiled.computePipelines  = computePipelines;
    compiled.graphicsPipelines = graphicsPipelines;

    compiled.compiledPasses.resize(passes.size());
    for (uint32_t passId = 0; passId < passes.size(); ++passId) {
        CompiledPass& pass      = compiled.compiledPasses[passId];
        pass.pass               = { .id = passId };
        pass.name               = passes[passId].name;
        pass.type               = passes[passId].type;
        pass.imageAccesses      = passes[passId].imageAccesses;
        pass.bufferAccesses     = passes[passId].bufferAccesses;
        pass.descriptorBindings = passes[passId].descriptorBindings;
    }

    for (uint32_t i = 0; i < computePipelines.size(); ++i)
        if (computePipelines[i].passId != InvalidHandle)
            compiled.compiledPasses[computePipelines[i].passId].pipelineId = i;
    for (uint32_t i = 0; i < graphicsPipelines.size(); ++i)
        if (graphicsPipelines[i].passId != InvalidHandle)
            compiled.compiledPasses[graphicsPipelines[i].passId].pipelineId = i;

    return compiled;
}

}
