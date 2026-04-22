#pragma once

#include <vector>
#include <string>

#include "builder_resource.hpp"
#include "compiled_resource.hpp"

class GraphicsPassBuilder;
// class ComputePassBuilder;
class TransferPassBuilder;
class PresentPassBuilder;

class RenderGraphBuilder {
public:
    ImageHandle createImage(std::string name, VkFormat format, uint32_t width, uint32_t height, uint32_t depth = 1, uint32_t arrayLayers = 1, uint32_t mipLevels = 1);

    GraphicsPassBuilder addGraphicsPass(std::string name);
    // ComputePassBuilder  addComputePass(std::string name);
    TransferPassBuilder addTransferPass(std::string name);
    PresentPassBuilder  addPresentPass(std::string name);

    const std::vector<PassInfo> getPasses() const { return passes; }
    const std::vector<ImageInfo> getImages() const { return images; }

    CompiledRenderGraph compile();  // TODO remove the returned compiled graph (only here for debugging)

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

    CompiledRenderGraph buildImageChains();

    std::vector<PassInfo> passes;
    std::vector<ImageInfo> images;

    friend GraphicsPassBuilder;
    // friend ComputePassBuilder;
    friend TransferPassBuilder;
    friend PresentPassBuilder;
};
