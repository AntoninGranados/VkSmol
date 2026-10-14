#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "builder_resource.hpp"
#include "compiled_resource.hpp"
#include "render_graph_builder.hpp"
#include "pass_context.hpp"
#include "pass/compute_pass_builder.hpp"
#include "pass/graphics_pass_builder.hpp"
#include "pass/transfer_pass_builder.hpp"

#include "../command/command_buffer.hpp"
#include "../render/dynamic_renderer.hpp"
#include "../render/descriptor/descriptor_set_layout.hpp"
#include "../render/descriptor/descriptor_set_group.hpp"
#include "../render/descriptor/descriptor_writer.hpp"
#include "../render/pipeline/compute_pipeline.hpp"
#include "../render/pipeline/graphics_pipeline.hpp"
#include "../render/pipeline/pipeline_cache.hpp"
#include "../sync/fence.hpp"
#include "../sync/semaphore.hpp"
#include "../utils.hpp"

namespace VkSmol {

class Engine;
class Device;
class LogicalDevice;

class RenderGraph {
public:
    explicit RenderGraph(Engine& engine);
    ~RenderGraph();

    RenderGraph(const RenderGraph&) = delete;
    RenderGraph& operator=(const RenderGraph&) = delete;
    RenderGraph(RenderGraph&&) = delete;
    RenderGraph& operator=(RenderGraph&&) = delete;

    ImageHandle  createImage(std::string name, VkFormat format, uint32_t width, uint32_t height);
    BufferHandle createBuffer(std::string name, size_t size);

    ComputePassBuilder  addComputePass(std::string name);
    GraphicsPassBuilder addGraphicsPass(std::string name);
    TransferPassBuilder addTransferPass(std::string name);
    void present(ImageHandle image);

    [[nodiscard]] std::vector<std::string> bake(std::string pipelineCachePath = {});
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

    std::optional<ImageHandle> presentedImage;
    VkPipelineStageFlags2 acquireWaitStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

    uint32_t frame = 0;
    std::array<CommandBuffer, MAX_FRAME_IN_FLIGHT> commandBuffers;
    std::array<Fence, MAX_FRAME_IN_FLIGHT> fences;
    std::array<Semaphore, MAX_FRAME_IN_FLIGHT> renderFinishedSemaphores;
    std::array<bool, MAX_FRAME_IN_FLIGHT> recordedSlots = {};

    DynamicRenderer dynamicRenderer;

    VkQueryPool          timestampPool    = VK_NULL_HANDLE;
    uint32_t             numTimestamps    = 0;
    float                timestampPeriod  = 1.0f;
    std::vector<double>  cachedTimestamps;

    std::unordered_map<uint32_t, ComputePipeline>  ownedComputePipelines;
    std::unordered_map<uint32_t, GraphicsPipeline> ownedGraphicsPipelines;
    std::unordered_map<uint32_t, uint32_t>         computePushConstantSizes;
    std::unordered_map<uint32_t, uint32_t>         graphicsPushConstantSizes;
    std::unordered_map<uint32_t, VkShaderStageFlags> graphicsPushConstantStages;
    PipelineCache pipelineCache;

    std::unordered_map<uint32_t, std::vector<DescriptorSetLayout>> passDescriptorLayouts;
    std::unordered_map<uint32_t, std::vector<DescriptorSetGroup>>  passDescriptorGroups;
    std::unordered_map<uint32_t, std::vector<uint64_t>>            writtenDescriptors;

    void resolveBindings(std::vector<std::string>& errors);

    void initDescriptorSets();
    void destroyDescriptorSets();
    DescriptorSetGroup createDescriptorSetGroup(DescriptorSetLayout& layout, std::string name = {});
    void updateDescriptorSetGroup(DescriptorSetGroup& group, uint32_t frameIndex, const DescriptorWriter& writer);
    void destroyDescriptorSetGroup(DescriptorSetGroup& group);
    std::vector<uint64_t> resolveDescriptors(const CompiledPass& pass) const;
    void updateDescriptors(const CompiledPass& pass, uint32_t frameIndex);
    void bindDescriptors(const CommandBuffer& commandBuffer, const CompiledPass& pass);

    std::vector<std::string> initPipelines();
    void destroyPipelines();
    const std::vector<uint32_t>& compilePassShader(const CompiledPass& pass, VkShaderStageFlagBits stage, const std::string& path, const ShaderDefines& defines, std::vector<std::string>& errors);

    void initSync();
    void destroySync();
    void initTimestamps();
    void readTimestamps();

    void recordPass(const CommandBuffer& commandBuffer, const CompiledPass& pass);
    void emitBarriers(const CommandBuffer& commandBuffer, const CompiledPass& pass);
    std::optional<ImageAccess> findColorAttachment(const CompiledPass& pass) const;

    friend class PassContext;
};

}
