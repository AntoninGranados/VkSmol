#pragma once

#include <vulkan/vulkan.h>

#include <unordered_map>
#include <vector>

#include "../command/command_buffer.hpp"
#include "compiled_resource.hpp"

struct PassHandle;
struct ImageHandle;

class RenderGraphBuilder;
struct PassImageBarrier;

struct ImageBinding {
    VkImage     image;
    VkImageView view;
};

struct AttachmentInfo {
    VkImageView        view;
    VkImageLayout      layout;
    VkAttachmentLoadOp loadOp;
};

class RenderGraphExecutor {
public:
    void setCompiledGraph(CompiledRenderGraph graph);
    
    // Update a binding (call per-frame for the swapchain image, or after resize for all images)
    void bindImage(const ImageHandle& handle, VkImage image, VkImageView view);

    void emitBarriers(const CommandBuffer& commandBuffer, const PassHandle& passHandle) const;
    void emitOutputBarriers(const CommandBuffer& commandBuffer) const;

    std::vector<AttachmentInfo> getColorAttachment(const PassHandle& passHandle) const;

    ImageBinding getTransferSrc(const PassHandle& passHandle) const;
    ImageBinding getTransferDst(const PassHandle& passHandle) const;


private:
    CompiledRenderGraph graph;
    std::unordered_map<uint32_t, ImageBinding> bindings;

    void emitBarrier(const CommandBuffer& commandBuffer, const PassImageBarrier& barrier) const;
};
