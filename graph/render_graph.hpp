#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "builder_resource.hpp"
#include "compiled_resource.hpp"
#include "pass/compute_pass_builder.hpp"
#include "pass/graphics_pass_builder.hpp"
#include "pass/transfer_pass_builder.hpp"
#include "pass_context.hpp"
#include "pass_descriptors.hpp"
#include "pass_pipelines.hpp"
#include "render_graph_builder.hpp"

#include "../command/command_buffer.hpp"
#include "../render/dynamic_renderer.hpp"
#include "../sync/fence.hpp"
#include "../sync/semaphore.hpp"
#include "../utils.hpp"

namespace VkSmol {

class Engine;
class Device;
class LogicalDevice;

struct SwapchainRelative {
    float scale = 1.0f;
};

class RenderGraph {
public:
    explicit RenderGraph(Engine& engine);
    ~RenderGraph();

    RenderGraph(const RenderGraph&) = delete;
    RenderGraph& operator=(const RenderGraph&) = delete;
    RenderGraph(RenderGraph&&) = delete;
    RenderGraph& operator=(RenderGraph&&) = delete;

    ImageHandle createImage(
        std::string name, VkFormat format, uint32_t width, uint32_t height, uint32_t layers = 1, uint32_t mips = 1
    );
    ImageHandle
    createImage(std::string name, VkFormat format, SwapchainRelative size, uint32_t layers = 1, uint32_t mips = 1);
    BufferHandle createBuffer(std::string name, size_t size);

    ComputePassBuilder  addComputePass(std::string name);
    GraphicsPassBuilder addGraphicsPass(std::string name);
    TransferPassBuilder addTransferPass(std::string name);
    void present(ImageHandle image);

    void bake(std::string pipelineCachePath = {});
    void execute();

    double getTimestampMs(const std::string& passName) const;
    std::string dump() const;

private:
    Engine& engine;
    Device& device;
    const LogicalDevice& logicalDevice;

    RenderGraphBuilder builder;
    CompiledRenderGraph graph;
    bool baked = false;

    std::vector<ImageHandle>  ownedImages;
    std::vector<BufferHandle> ownedBuffers;
    std::vector<std::pair<ImageHandle, SwapchainRelative>> relativeImages;

    std::optional<ImageHandle> presentedImage;
    VkPipelineStageFlags2 acquireWaitStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

    uint32_t frame = 0;
    std::array<CommandBuffer, MAX_FRAME_IN_FLIGHT> commandBuffers;
    std::array<Fence, MAX_FRAME_IN_FLIGHT> fences;
    std::array<bool, MAX_FRAME_IN_FLIGHT> recordedSlots = {};

    DynamicRenderer dynamicRenderer;
    PassDescriptors descriptors;
    PassPipelines pipelines;

    VkQueryPool          timestampPool    = VK_NULL_HANDLE;
    uint32_t             numTimestamps    = 0;
    float                timestampPeriod  = 1.0f;
    std::vector<double>  cachedTimestamps;

    VkExtent2D relativeExtent(SwapchainRelative size) const;
    void resizeRelativeImages();
    void initSync();
    void destroySync();
    void initTimestamps();
    void readTimestamps();

    void recordPass(const CommandBuffer& commandBuffer, const CompiledPass& pass);
    void recordGraphicsPass(const CommandBuffer& commandBuffer, const CompiledPass& pass, const PassInfo& info);
    void emitBarriers(const CommandBuffer& commandBuffer, const CompiledPass& pass);

    friend class PassContext;
};

}
