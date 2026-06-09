#pragma once

#include <vulkan/vulkan.h>

#include <unordered_map>
#include <vector>

#include "../command/command_buffer.hpp"
#include "../image/image.hpp"
#include "../image/image_view.hpp"
#include "../image/sampler.hpp"
#include "compiled_resource.hpp"

struct PassHandle;
struct ImageHandle;

class RenderGraphBuilder;
struct PassImageBarrier;
class VkSmol;

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

    // Lifecycle for executor-owned (Transient + Managed) resources
    void init(VkSmol& engine);
    void destroy(VkSmol& engine);
    void resize(VkSmol& engine, uint32_t width, uint32_t height);

    // Swap the bindings of two handles (ping-pong)
    void swapBindings(const ImageHandle& a, const ImageHandle& b);

    // Manually bind an external (Imported) image each frame
    void bindImage(const ImageHandle& handle, VkImage image, VkImageView view);

    const ImageBinding& getBinding(const ImageHandle& handle) const;
    Image&              getImage(const ImageHandle& handle);
    const ImageView&    getView(const ImageHandle& handle) const;
    const Sampler&      getSampler(const ImageHandle& handle) const;

    void emitBarriers(const CommandBuffer& commandBuffer, const PassHandle& passHandle) const;
    void emitOutputBarriers(const CommandBuffer& commandBuffer) const;

    std::vector<AttachmentInfo> getColorAttachment(const PassHandle& passHandle) const;

    ImageBinding getTransferSrc(const PassHandle& passHandle) const;
    ImageBinding getTransferDst(const PassHandle& passHandle) const;

private:
    CompiledRenderGraph graph;
    std::unordered_map<uint32_t, ImageBinding> bindings;
    std::unordered_map<uint32_t, Image>      ownedImages;
    std::unordered_map<uint32_t, ImageView>  ownedViews;
    std::unordered_map<uint32_t, Sampler>    ownedSamplers;

    void emitBarrier(const CommandBuffer& commandBuffer, const PassImageBarrier& barrier) const;
};
