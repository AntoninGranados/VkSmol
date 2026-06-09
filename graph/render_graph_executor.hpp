#pragma once

#include <vulkan/vulkan.h>

#include <unordered_map>
#include <optional>
#include <vector>

#include "../command/command_buffer.hpp"
#include "../memory/buffer.hpp"
#include "../utils.hpp"
#include "../image/image.hpp"
#include "../image/image_view.hpp"
#include "../image/sampler.hpp"
#include "../render/pipeline/compute_pipeline.hpp"
#include "../render/pipeline/graphics_pipeline.hpp"
#include "../sync/fence.hpp"
#include "../sync/semaphore.hpp"
#include "../frame_context.hpp"
#include "compiled_resource.hpp"

struct PassHandle;
struct ImageHandle;
struct BufferHandle;
struct SubmissionGroupHandle;

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

    // Frame lifecycle — replaces engine.beginFrame() / endFrame()
    std::optional<FrameContext> beginFrame(VkSmol& engine);
    void present(VkSmol& engine);

    // Per-submission-group recording and submission
    CommandBuffer& beginRecording(const SubmissionGroupHandle& group, VkSmol& engine);
    void           endRecording(const SubmissionGroupHandle& group, VkSmol& engine);

    // Swap the bindings of two handles (ping-pong)
    void swapBindings(const ImageHandle& a, const ImageHandle& b);

    // Manually bind an external (Imported) image each frame
    void bindImage(const ImageHandle& handle, VkImage image, VkImageView view);

    const ImageBinding& getBinding(const ImageHandle& handle) const;
    Image&              getImage(const ImageHandle& handle);
    const ImageView&    getView(const ImageHandle& handle) const;
    const Sampler&      getSampler(const ImageHandle& handle) const;

    // Manually bind an external (Imported) buffer
    void bindBuffer(const BufferHandle& handle, VkBuffer buffer);
    // For single buffers use frameIndex = UINT32_MAX (resolves to currentFrame for per-frame buffers)
    Buffer& getBuffer(const BufferHandle& handle, uint32_t frameIndex = UINT32_MAX);
    // Resize a managed single buffer (destroys and recreates with new byte size)
    void resizeBuffer(VkSmol& engine, const BufferHandle& handle, size_t newSize);

    ComputePipeline&  getComputePipeline(ComputePipelineHandle handle);
    GraphicsPipeline& getGraphicsPipeline(GraphicsPipelineHandle handle);
    // Rebuild all pipelines from their registered shader paths — safe to call on a live executor
    void reloadPipelines(VkSmol& engine);

    void emitBarriers(const CommandBuffer& commandBuffer, const PassHandle& passHandle) const;
    void emitOutputBarriers(const CommandBuffer& commandBuffer) const;

    std::vector<AttachmentInfo> getColorAttachment(const PassHandle& passHandle) const;

    ImageBinding getTransferSrc(const PassHandle& passHandle) const;
    ImageBinding getTransferDst(const PassHandle& passHandle) const;

private:
    CompiledRenderGraph graph;

    std::unordered_map<uint32_t, ImageBinding> bindings;
    std::unordered_map<uint32_t, Image>        ownedImages;
    std::unordered_map<uint32_t, ImageView>    ownedViews;
    std::unordered_map<uint32_t, Sampler>      ownedSamplers;

    std::unordered_map<uint32_t, VkBuffer> bufferBindings;
    std::unordered_map<uint32_t, Buffer>   ownedBuffers;
    std::unordered_map<uint32_t, std::array<Buffer, MAX_FRAME_IN_FLIGHT>> ownedPerFrameBuffers;

    std::unordered_map<uint32_t, ComputePipeline>  ownedComputePipelines;
    std::unordered_map<uint32_t, GraphicsPipeline> ownedGraphicsPipelines;

    // Sync and command buffers — [groupId][frame]
    std::vector<Semaphore>                   imageAvailableSemaphores;
    std::vector<std::vector<Semaphore>>      groupSemaphores;
    std::vector<std::vector<Fence>>          groupFences;
    std::vector<std::vector<CommandBuffer>>  groupCommandBuffers;

    uint32_t currentFrame = 0;

    void initImages(VkSmol& engine);
    void destroyImages(VkSmol& engine);
    void initBuffers(VkSmol& engine);
    void destroyBuffers(VkSmol& engine);
    void initPerFrameBuffers(VkSmol& engine);
    void destroyPerFrameBuffers(VkSmol& engine);
    void initPipelines(VkSmol& engine);
    void destroyPipelines(VkSmol& engine);
    void initSync(VkSmol& engine);
    void destroySync(VkSmol& engine);

    void emitBarrier(const CommandBuffer& commandBuffer, const PassImageBarrier& barrier) const;
};
