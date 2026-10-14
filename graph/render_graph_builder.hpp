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
    // TODO: add support for 'ImageSubresourceRangeInfo'

    ImageHandle createImage(
        std::string name, VkFormat format,
        uint32_t width, uint32_t height, uint32_t depth = 1,
        ImageOwnership ownership = ImageOwnership::Transient,
        VkImageUsageFlags extraUsageFlags = 0,
        std::optional<ImageAccessInfo> initialAccess = {},
        std::optional<ImageAccessInfo> finalAccess = {}
    );

    BufferHandle createBuffer(
        std::string name, size_t size,
        BufferOwnership ownership = BufferOwnership::Managed,
        VkBufferUsageFlags extraUsageFlags = 0,
        const void* initialData = nullptr
    );

    GraphicsPassBuilder addGraphicsPass(std::string name);
    ComputePassBuilder  addComputePass(std::string name);
    TransferPassBuilder addTransferPass(std::string name);
    PassHandle          addPresentPass(ImageHandle image);

    const std::vector<PassInfo>&  getPasses()  const { return passes; }
    const std::vector<ImageInfo>& getImages()  const { return images; }
    const std::vector<BufferInfo>& getBuffers() const { return buffers; }

    CompiledRenderGraph compile();

private:
    template <class TPassBuilder, PassType T>
    TPassBuilder addPass(std::string name) {
        PassHandle handle = { .id = static_cast<uint32_t>(passes.size()) };
        passes.push_back({ .name = std::move(name), .type = T });
        return TPassBuilder(handle, this);
    }

    std::vector<PassInfo>             passes;
    std::vector<ImageInfo>            images;
    std::vector<BufferInfo>           buffers;
    std::vector<ComputePipelineInfo>  computePipelines;
    std::vector<GraphicsPipelineInfo> graphicsPipelines;

    template <class Derived> friend class PassBuilder;
    friend GraphicsPassBuilder;
    friend ComputePassBuilder;
    friend TransferPassBuilder;
    friend RenderGraph;
};

}
