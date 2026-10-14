#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <cstddef>
#include <optional>
#include <ranges>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include "builder_resource.hpp"
#include "compiled_resource.hpp"
#include "render_graph_builder.hpp"
#include "pass_context.hpp"
#include "pass/compute_pass_builder.hpp"
#include "pass/graphics_pass_builder.hpp"
#include "pass/transfer_pass_builder.hpp"

#include "../command/command_buffer.hpp"
#include "../image/image.hpp"
#include "../image/image_view.hpp"
#include "../image/sampler.hpp"
#include "../memory/buffer.hpp"
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

struct ImageBinding {
    VkImage     image;
    VkImageView view;
};

// TODO: allow the use of a single buffer for better caching and use the offset parameters

class RenderGraph {
public:
    explicit RenderGraph(Engine& engine);
    ~RenderGraph();

    RenderGraph(const RenderGraph&) = delete;
    RenderGraph& operator=(const RenderGraph&) = delete;
    RenderGraph(RenderGraph&&) = delete;
    RenderGraph& operator=(RenderGraph&&) = delete;

    ImageHandle  createImage(std::string name, VkFormat format, uint32_t width, uint32_t height);
    BufferHandle createBuffer(std::string name, size_t size, const void* initialData = nullptr);
    ImageHandle  importSwapchain();

    ComputePassBuilder  addComputePass(std::string name);
    GraphicsPassBuilder addGraphicsPass(std::string name);
    TransferPassBuilder addTransferPass(std::string name);
    void present(ImageHandle image);

    [[nodiscard]] std::vector<std::string> bake(std::string pipelineCachePath = {});
    void execute();

    void resizeImage(ImageHandle handle, uint32_t width, uint32_t height);
    bool resizeBuffer(BufferHandle handle, size_t newSize);

    void writeBuffer(BufferHandle handle, const void* data, size_t size, size_t offset = 0);
    template <typename T>
        requires(std::is_trivially_copyable_v<T> && !std::ranges::range<T>)
    void writeBuffer(BufferHandle handle, const T& value, size_t offset = 0) {
        writeBuffer(handle, &value, sizeof(T), offset);
    }
    template <std::ranges::contiguous_range R>
    void writeBuffer(BufferHandle handle, const R& values, size_t offset = 0) {
        writeBuffer(handle, std::ranges::data(values), std::ranges::size(values) * sizeof(std::ranges::range_value_t<R>), offset);
    }

    void readBuffer(BufferHandle handle, void* out, size_t size, size_t offset = 0);
    template <std::ranges::contiguous_range R>
    void readBuffer(BufferHandle handle, R& out, size_t offset = 0) {
        readBuffer(handle, std::ranges::data(out), std::ranges::size(out) * sizeof(std::ranges::range_value_t<R>), offset);
    }

    const ImageView& getView(ImageHandle handle) const;
    const Sampler& getSampler(ImageHandle handle) const;

    double getTimestampMs(const std::string& passName) const;
    std::string dump() const;

private:
    Engine& engine;
    Device& device;
    const LogicalDevice& logicalDevice;

    RenderGraphBuilder builder;
    CompiledRenderGraph graph;
    bool baked = false;

    std::optional<ImageHandle> swapchainImage;
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

    std::unordered_map<uint32_t, ImageBinding> bindings;
    std::unordered_map<uint32_t, Image>        ownedImages;
    std::unordered_map<uint32_t, ImageView>    ownedViews;
    std::unordered_map<uint32_t, Sampler>      ownedSamplers;

    std::unordered_map<uint32_t, VkBuffer> bufferBindings;
    std::unordered_map<uint32_t, Buffer>   ownedBuffers;

    struct PendingBufferWrite {
        VkBuffer dst;
        size_t offset;
        std::vector<std::byte> data;
    };
    struct PendingBufferReplace {
        Buffer previous;
        VkBuffer next;
        size_t preservedSize;
    };
    std::vector<std::variant<PendingBufferWrite, PendingBufferReplace>> pendingBufferTransfers;

    struct StagingArena {
        Buffer buffer;
        size_t used = 0;
    };
    std::array<StagingArena, MAX_FRAME_IN_FLIGHT> stagingArenas;

    std::unordered_map<uint32_t, ComputePipeline>  ownedComputePipelines;
    std::unordered_map<uint32_t, GraphicsPipeline> ownedGraphicsPipelines;
    std::unordered_map<uint32_t, uint32_t>         computePushConstantSizes;
    std::unordered_map<uint32_t, uint32_t>         graphicsPushConstantSizes;
    PipelineCache pipelineCache;

    std::unordered_map<uint32_t, std::vector<DescriptorSetLayout>> passDescriptorLayouts;
    std::unordered_map<uint32_t, std::vector<DescriptorSetGroup>>  passDescriptorGroups;
    std::unordered_set<uint32_t>                                   dirtyDescriptors;

    void resolveBindings(std::vector<std::string>& errors);

    void initImages();
    void destroyImages();
    VkImageUsageFlags imageUsageFlags(const CompiledRenderGraph& compiled, uint32_t imageId) const;

    void initBuffers();
    void destroyBuffers();
    VkBufferUsageFlags bufferUsageFlags(const CompiledRenderGraph& compiled, uint32_t bufferId) const;
    void replaceBuffer(uint32_t bufferId, size_t newSize, bool preserve);
    size_t stage(const void* data, size_t size);
    void flushBufferTransfers(const CommandBuffer& commandBuffer);
    void discardBufferTransfers();

    void initDescriptorSets();
    void destroyDescriptorSets();
    DescriptorSetGroup createDescriptorSetGroup(DescriptorSetLayout& layout, std::string name = {});
    void updateDescriptorSetGroup(DescriptorSetGroup& group, uint32_t frameIndex, const DescriptorWriter& writer);
    void destroyDescriptorSetGroup(DescriptorSetGroup& group);
    void updateDescriptors(const PassHandle& pass, uint32_t frameIndex);
    void refreshDescriptorsForImage(uint32_t imageId, uint32_t frameIndex);
    void refreshDescriptorsForBuffer(uint32_t bufferId);
    void bindDescriptors(const CommandBuffer& commandBuffer, const CompiledPass& pass);

    std::vector<std::string> initPipelines();
    void destroyPipelines();
    const std::vector<uint32_t>& compilePassShader(const CompiledPass& pass, VkShaderStageFlagBits stage, const std::string& path, const ShaderDefines& defines, std::vector<std::string>& errors);

    void initSync();
    void destroySync();
    void initTimestamps();
    void readTimestamps();

    void recordPass(const CommandBuffer& commandBuffer, const CompiledPass& pass);
    void emitBarriers(const CommandBuffer& commandBuffer, const CompiledPass& pass) const;
    void emitOutputBarriers(const CommandBuffer& commandBuffer) const;
    void emitBarrier(const CommandBuffer& commandBuffer, const PassImageBarrier& barrier) const;
    std::optional<ImageAccess> findColorAttachment(const CompiledPass& pass) const;
    VkExtent2D getImageExtent(uint32_t imageId) const;

    friend class PassContext;
};

}
