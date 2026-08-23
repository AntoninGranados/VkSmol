#pragma once

#include <vulkan/vulkan.h>

#include <vector>
#include <string>

#include "builder_resource.hpp"

class GraphicsPassBuilder;
class ComputePassBuilder;
class TransferPassBuilder;
class PresentPassBuilder;

struct CompiledRenderGraph;

class RenderGraphBuilder {
public:
    SubmissionGroupHandle addSubmissionGroup(std::string name);

    // TODO: add support for 'ImageSubresourceRangeInfo'

    ImageHandle createImage(
        std::string name, VkFormat format,
        uint32_t width, uint32_t height, uint32_t depth = 1,
        VkSmolImageOwnership ownership = VKSMOL_IMAGE_OWNERSHIP_TRANSIENT,
        VkImageUsageFlags extraUsageFlags = 0,
        std::optional<ImageAccessInfo> initialAccess = {},
        std::optional<ImageAccessInfo> finalAccess = {}
    );

    BufferHandle createBuffer(
        std::string name, size_t size,
        VkSmolBufferCreateFlags flags = 0,
        VkBufferUsageFlags extraUsageFlags = 0,
        const void* initialData = nullptr
    );

    GraphicsPassBuilder addGraphicsPass(std::string name);
    ComputePassBuilder  addComputePass(std::string name);
    TransferPassBuilder addTransferPass(std::string name);
    PresentPassBuilder  addPresentPass(std::string name);

    const std::vector<PassInfo>&  getPasses()  const { return passes; }
    const std::vector<ImageInfo>& getImages()  const { return images; }
    const std::vector<BufferInfo>& getBuffers() const { return buffers; }

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

    std::vector<SubmissionGroupInfo>  groups;
    std::vector<PassInfo>             passes;
    std::vector<ImageInfo>            images;
    std::vector<BufferInfo>           buffers;
    std::vector<ComputePipelineInfo>  computePipelines;
    std::vector<GraphicsPipelineInfo> graphicsPipelines;
    uint32_t nextTimestampIndex = 0;

    friend class PassBuilder;
    friend GraphicsPassBuilder;
    friend ComputePassBuilder;
    friend TransferPassBuilder;
    friend PresentPassBuilder;
};
