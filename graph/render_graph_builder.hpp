#pragma once

#include <vulkan/vulkan.h>

#include <vector>
#include <string>

#include "builder_resource.hpp"

class GraphicsPassBuilder;
// class ComputePassBuilder;
class TransferPassBuilder;
class PresentPassBuilder;

struct CompiledRenderGraph;


class RenderGraphBuilder {
public:
    // @todo add support for 'ImageSubresourceRangeInfo'
    ImageHandle createImage(std::string name, VkFormat format, uint32_t width, uint32_t height, uint32_t depth = 1);
    ImageHandle importImage(std::string name, VkFormat format, uint32_t width, uint32_t height, uint32_t depth = 1, ImageAccessInfo initialAccess = {}, std::optional<ImageAccessInfo> finalAccess = {});

    GraphicsPassBuilder addGraphicsPass(std::string name);
    // ComputePassBuilder  addComputePass(std::string name);
    TransferPassBuilder addTransferPass(std::string name);
    PresentPassBuilder  addPresentPass(std::string name);

    const std::vector<PassInfo>& getPasses() const { return passes; }
    const std::vector<ImageInfo>&getImages() const { return images; }

    // @todo remove the returned compiled graph (only here for debugging)
    CompiledRenderGraph compile();

private:
    template <class TPassBuilder, PassType T>
    TPassBuilder addPass(std::string name) {
        PassHandle handle = { .id = static_cast<uint32_t>(passes.size()) };
        PassInfo info = {
            .name = std::move(name),
            .type = T
        };
        passes.push_back(info);

        return TPassBuilder(handle, this);
    }

    std::vector<PassInfo> passes;
    std::vector<ImageInfo> images;

    friend GraphicsPassBuilder;
    // friend ComputePassBuilder;
    friend TransferPassBuilder;
    friend PresentPassBuilder;
};
