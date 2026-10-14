#pragma once

#include <vulkan/vulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <unordered_map>
#include <unordered_set>
#include <array>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "./device/device.hpp"

#include "./present/window.hpp"
#include "./present/swapchain.hpp"
#include "./render/dynamic_renderer.hpp"

#include "./render/descriptor/descriptor_set_layout.hpp"
#include "./render/descriptor/descriptor_pool.hpp"
#include "./render/descriptor/descriptor_set.hpp"
#include "./render/descriptor/descriptor_set_group.hpp"
#include "./render/descriptor/descriptor_writer.hpp"

#include "./render/shader.hpp"
#include "./render/shader_file_system.hpp"
#include "./render/pipeline/graphics_pipeline.hpp"
#include "./render/pipeline/compute_pipeline.hpp"
#include "./render/pipeline/pipeline_cache.hpp"
#include "./command/command_pool.hpp"
#include "./command/command_buffer.hpp"

#include "./memory/buffer.hpp"

#include "./sync/semaphore.hpp"
#include "./sync/fence.hpp"

#include "./image/image.hpp"
#include "./image/image_view.hpp"
#include "./image/sampler.hpp"

#include "./utils.hpp"
#include "./frame_context.hpp"
#include "./graph/compiled_resource.hpp"

namespace VkSmol {

// TODO: allow the use of a single buffer for better caching and use the offset parameters

struct ImageBinding {
    VkImage     image;
    VkImageView view;
};

struct AttachmentInfo {
    VkImageView        view;
    VkImageLayout      layout;
    VkAttachmentLoadOp loadOp;
    VkExtent2D         extent;
};

class RenderGraphBuilder;

class Engine {
public:
    Engine(const std::string& appName, uint32_t appVersion, std::optional<Window&> window = std::nullopt);
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) = delete;
    Engine& operator=(Engine&&) = delete;

    bool isHeadless() const { return !window.has_value(); }

    Device& getDevice() { return device; }

    void setShaderFileSystem(std::shared_ptr<ShaderFileSystem> fileSystem) { device.setShaderFileSystem(std::move(fileSystem)); }
    ShaderLocation getShaderLocation(const std::string& shaderPath, const std::string& parameterName, const ShaderDefines& defines = {}) {
        return device.getShaderLocation(shaderPath, parameterName, defines);
    }

    void setGraph(RenderGraphBuilder& builder);
    [[nodiscard]] std::vector<std::string> initGraph(std::string pipelineCachePath = {});
    [[nodiscard]] std::vector<std::string> rebuildGraph(RenderGraphBuilder& builder, std::string pipelineCachePath = {});
    void destroyGraph();

    std::optional<FrameContext> beginFrame();
    void advanceFrame();
    void present();
    CommandBuffer& beginRecording(const SubmissionGroupHandle& group);
    void  endRecording(const SubmissionGroupHandle& group);

    void bindImage(const ImageHandle& handle, VkImage image, VkImageView view);
    void swapBindings(const ImageHandle& a, const ImageHandle& b);
    const ImageBinding& getBinding(const ImageHandle& handle) const;
    Image& getImage(const ImageHandle& handle);
    const ImageView& getView(const ImageHandle& handle) const;
    const Sampler& getSampler(const ImageHandle& handle) const;
    void resizeImage(ImageHandle handle, uint32_t width, uint32_t height);

    void bindBuffer(const BufferHandle& handle, VkBuffer buffer);
    Buffer& getBuffer(const BufferHandle& handle);
    bool resizeBuffer(const BufferHandle& handle, size_t newSize);

    void writeBuffer(const BufferHandle& handle, const void* data, size_t size, size_t offset = 0);
    template <typename T>
        requires(std::is_trivially_copyable_v<T> && !std::ranges::range<T>)
    void writeBuffer(const BufferHandle& handle, const T& value, size_t offset = 0) {
        writeBuffer(handle, &value, sizeof(T), offset);
    }
    template <std::ranges::contiguous_range R>
    void writeBuffer(const BufferHandle& handle, const R& values, size_t offset = 0) {
        writeBuffer(handle, std::ranges::data(values), std::ranges::size(values) * sizeof(std::ranges::range_value_t<R>), offset);
    }

    void readBuffer(const BufferHandle& handle, void* out, size_t size, size_t offset = 0);
    template <std::ranges::contiguous_range R>
    void readBuffer(const BufferHandle& handle, R& out, size_t offset = 0) {
        readBuffer(handle, std::ranges::data(out), std::ranges::size(out) * sizeof(std::ranges::range_value_t<R>), offset);
    }

    ComputePipeline&  getComputePipeline(ComputePipelineHandle handle);
    GraphicsPipeline& getGraphicsPipeline(GraphicsPipelineHandle handle);

    void dispatch(const CommandBuffer& cmd, const PassHandle& passHandle, uint32_t x, uint32_t y);

    void beginGraphics(const CommandBuffer& cmd, const PassHandle& passHandle, const float clearColor[4] = nullptr);
    void endGraphics(const CommandBuffer& cmd, const PassHandle& passHandle);

    double getTimestampMs(TimestampHandle handle) const;

    void emitBarriers(const CommandBuffer& commandBuffer, const PassHandle& passHandle) const;
    void emitOutputBarriers(const CommandBuffer& commandBuffer) const;
    std::vector<AttachmentInfo> getColorAttachment(const PassHandle& passHandle) const;
    ImageBinding getTransferSrc(const PassHandle& passHandle) const;
    ImageBinding getTransferDst(const PassHandle& passHandle) const;

    void bindDescriptors(const CommandBuffer& commandBuffer, const PassHandle& pass);

    void submit(
        CommandBuffer& commandBuffer,
        Fence& fence,
        const std::vector<const Semaphore*>& waitSemaphores,
        const std::vector<VkPipelineStageFlags2>& waitStages,
        const std::vector<const Semaphore*>& signalSemaphores
    );

    void waitIdle() { device.waitIdle(); }

    void destroyGraphicsPipeline(GraphicsPipeline& graphicsPipeline);
    void destroyComputePipeline(ComputePipeline& computePipeline);

    void beginDynamicRenderer(
        const CommandBuffer& commandBuffer, 
        VkImageView imageView, VkImageLayout imageLayout,
        VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp,
        VkExtent2D extent,
        VkClearColorValue clearColor = {{ 0.0f, 0.0f, 0.0f, 1.0f }}
    );
    void endDynamicRenderer(const CommandBuffer& commandBuffer);

    VkExtent2D getExtent() const { return swapchain ? swapchain->getExtent() : VkExtent2D{}; }
    uint32_t getFrame() { return currentFrame; }

    Image& getSwapchainImage(size_t imageIndex);
    ImageView& getSwapchainImageView(size_t imageIndex);

    std::string dumpGraph() const;


private:
    std::optional<Window&> window;
    Device device;
    const LogicalDevice& logicalDevice;
    std::optional<Swapchain> swapchain;

    std::vector<Semaphore> imageAvailableSemaphores;
    uint64_t swapchainGeneration = 0;
    bool framebufferResized = false;

    DynamicRenderer dynamicRenderer;

    DescriptorPool uiDescriptorPool;
    VkFormat uiColorFormat = VK_FORMAT_UNDEFINED;

    uint32_t currentFrame = 0, imageIndex;

    void initDescriptorSetLayout(DescriptorSetLayout& descriptorSetLayout);
    void destroyDescriptorSetLayout(DescriptorSetLayout& descriptorSetLayout);

    DescriptorSetGroup createDescriptorSetGroup(DescriptorSetLayout& layout, std::string name = {});
    void updateDescriptorSetGroup(DescriptorSetGroup& group, uint32_t frameIndex, const DescriptorWriter& writer);
    void destroyDescriptorSetGroup(DescriptorSetGroup& group);

    GraphicsPipeline initGraphicsPipeline(
        VkPipelineVertexInputStateCreateInfo vertexInputInfo,
        const std::vector<const Shader*>& shaders,
        const std::vector<const DescriptorSetLayout*>& setLayouts,
        GraphicsPipeline other = GraphicsPipeline(),
        VkFormat colorFormat = VK_FORMAT_UNDEFINED
    );
    ComputePipeline initComputePipeline(
        const Shader& shader,
        const std::vector<const DescriptorSetLayout*>& setLayouts,
        ComputePipeline other = {}
    );

    const std::vector<uint32_t>& compilePassShader(const CompiledPass& pass, VkShaderStageFlagBits stage, const std::string& path, const ShaderDefines& defines, std::vector<std::string>& errors);

    // Graph state
    CompiledRenderGraph graph;
    bool graphInitialized = false;

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
    PipelineCache pipelineCache;

    std::unordered_map<uint32_t, std::vector<DescriptorSetLayout>> passDescriptorLayouts;
    std::unordered_map<uint32_t, std::vector<DescriptorSetGroup>>  passDescriptorGroups;
    std::unordered_set<uint32_t>                      dirtyDescriptors;

    std::vector<std::vector<Semaphore>>     groupSemaphores;
    std::vector<std::vector<Fence>>         groupFences;
    std::vector<std::vector<CommandBuffer>> groupCommandBuffers;

    void initImages();
    void destroyImages();
    VkImageUsageFlags imageUsageFlags(const CompiledRenderGraph& compiled, uint32_t imageId) const;

    struct ReusedResources {
        std::unordered_map<uint32_t, Image>     images;
        std::unordered_map<uint32_t, ImageView> views;
        std::unordered_map<uint32_t, Sampler>   samplers;
        std::unordered_map<uint32_t, Buffer>    buffers;
    };
    ReusedResources takeReusableResources(CompiledRenderGraph& next);
    std::vector<std::string> initGraphState(std::string pipelineCachePath);

    void initBuffers();
    void destroyBuffers();
    VkBufferUsageFlags bufferUsageFlags(const CompiledRenderGraph& compiled, uint32_t bufferId) const;
    void replaceBuffer(uint32_t bufferId, size_t newSize, bool preserve);
    size_t stage(const void* data, size_t size);
    void flushBufferTransfers(const CommandBuffer& commandBuffer);
    void discardBufferTransfers();
    
    void initDescriptorSets();
    void destroyDescriptorSets();
    void updateDescriptors(const PassHandle& pass, uint32_t frameIndex);
    void refreshDescriptorsForImage(uint32_t imageId, uint32_t frameIndex);
    void refreshDescriptorsForBuffer(uint32_t bufferId);
    
    std::vector<std::string> initPipelines();
    void destroyPipelines();
    
    void initSync();
    void destroySync();

    void emitBarrier(const CommandBuffer& commandBuffer, const PassImageBarrier& barrier) const;

    void initImgui();
    void recreateSwapchain();
};

}
