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

    // Transient: executor creates it, starts UNDEFINED, first access must be a write
    ImageHandle createImage(std::string name, VkFormat format, uint32_t width, uint32_t height, uint32_t depth = 1, VkImageUsageFlags extraUsageFlags = 0);

    // Managed: executor creates it, starts in the declared initialAccess layout, can be read first
    ImageHandle createImage(std::string name, VkFormat format, uint32_t width, uint32_t height, uint32_t depth, ImageAccessInfo initialAccess, ImageAccessInfo finalAccess, VkImageUsageFlags extraUsageFlags = 0);

    // Imported: caller provides the VkImage/VkImageView each frame via bindImage
    ImageHandle importImage(std::string name, VkFormat format, uint32_t width, uint32_t height, uint32_t depth = 1, ImageAccessInfo initialAccess = {}, ImageAccessInfo finalAccess = {});

    // Managed: executor creates and owns a single buffer shared across all frames
    BufferHandle createBuffer(std::string name, size_t size, VkBufferUsageFlags extraUsageFlags = 0);

    // Static: executor uploads data once via staging at init; device-local, not CPU-writable afterward
    BufferHandle createStaticBuffer(std::string name, const void* data, size_t size, VkBufferUsageFlags extraUsageFlags = 0);

    // Managed, per-frame: executor creates MAX_FRAME_IN_FLIGHT copies (use for CPU-written UBOs)
    BufferHandle createPerFrameBuffer(std::string name, size_t size, VkBufferUsageFlags extraUsageFlags = 0);

    // Imported: caller provides the VkBuffer each frame via bindBuffer
    BufferHandle importBuffer(std::string name);

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

    friend class PassBuilder;
    friend GraphicsPassBuilder;
    friend ComputePassBuilder;
    friend TransferPassBuilder;
    friend PresentPassBuilder;
};
