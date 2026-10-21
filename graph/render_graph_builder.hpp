#pragma once

#include <vulkan/vulkan.h>

#include <vector>
#include <string>

#include "builder_resource.hpp"

namespace VkSmol {

class GraphicsPassBuilder;
class ComputePassBuilder;
class TransferPassBuilder;
class RenderGraph;
template <class Derived> class PassBuilder;

struct CompiledRenderGraph;

class RenderGraphBuilder {
public:
    GraphicsPassBuilder addGraphicsPass(std::string name);
    ComputePassBuilder  addComputePass(std::string name);
    TransferPassBuilder addTransferPass(std::string name);
    PassHandle          addPresentPass(ImageHandle image);

    const std::vector<PassInfo>& getPasses() const { return passes; }

    CompiledRenderGraph compile() const;

private:
    template <class TPassBuilder, PassType T>
    TPassBuilder addPass(std::string name) {
        PassHandle handle = { .id = static_cast<uint32_t>(passes.size()) };
        passes.push_back({ .name = std::move(name), .type = T });
        return TPassBuilder(handle, this);
    }

    std::vector<PassInfo>             passes;
    std::vector<ComputePipelineInfo>  computePipelines;
    std::vector<GraphicsPipelineInfo> graphicsPipelines;

    template <class Derived> friend class PassBuilder;
    friend GraphicsPassBuilder;
    friend ComputePassBuilder;
    friend TransferPassBuilder;
    friend RenderGraph;
};

}
