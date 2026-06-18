#pragma once

#include <vulkan/vulkan.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <unordered_map>
#include <unordered_set>
#include <array>
#include <functional>
#include <vector>

#include "./platform/window.hpp"

#include "./instance/instance.hpp"
#include "./instance/debug_messenger.hpp"
#include "./present/surface.hpp"

#include "./device/physical_device.hpp"
#include "./device/logical_device.hpp"
#include "./device/queue.hpp"

#include "./present/swapchain.hpp"
#include "./render/dynamic_renderer.hpp"

#include "./render/descriptor/descriptor_set_layout.hpp"
#include "./render/descriptor/descriptor_pool.hpp"
#include "./render/descriptor/descriptor_set.hpp"
#include "./render/descriptor/descriptor_set_group.hpp"
#include "./render/descriptor/descriptor_writer.hpp"

#include "./render/shader.hpp"
#include "./render/pipeline/graphics_pipeline.hpp"
#include "./render/pipeline/compute_pipeline.hpp"
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

// TODO: allow the use of a single buffer for better caching and use the offset parameters

struct ImageBinding {
    VkImage     image;
    VkImageView view;
};

struct AttachmentInfo {
    VkImageView        view;
    VkImageLayout      layout;
    VkAttachmentLoadOp loadOp;
};

class RenderGraphBuilder;

class VkSmol {
public:
    void init(std::string appName, uint32_t appVersion);

    void setGraph(RenderGraphBuilder& builder);
    void initGraph();
    void destroyGraph();

    std::optional<FrameContext> beginFrame();
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
    Buffer& getBuffer(const BufferHandle& handle, uint32_t frameIndex = UINT32_MAX);
    bool resizeBuffer(const BufferHandle& handle, size_t newSize);

    ComputePipeline&  getComputePipeline(ComputePipelineHandle handle);
    GraphicsPipeline& getGraphicsPipeline(GraphicsPipelineHandle handle);
    void reloadPipelines();

    void emitBarriers(const CommandBuffer& commandBuffer, const PassHandle& passHandle) const;
    void emitOutputBarriers(const CommandBuffer& commandBuffer) const;
    std::vector<AttachmentInfo> getColorAttachment(const PassHandle& passHandle) const;
    ImageBinding getTransferSrc(const PassHandle& passHandle) const;
    ImageBinding getTransferDst(const PassHandle& passHandle) const;

    void bindDescriptors(const CommandBuffer& commandBuffer, const PassHandle& pass);

    CommandBuffer allocCommandBuffer();
    void freeCommandBuffer(CommandBuffer& commandBuffer);

    void submit(
        CommandBuffer& commandBuffer,
        Fence& fence,
        const std::vector<const Semaphore*>& waitSemaphores,
        const std::vector<VkPipelineStageFlags>& waitStages,
        const std::vector<const Semaphore*>& signalSemaphores
    );

    const LogicalDevice& getDevice() const { return device; }

    void waitIdle();
    void terminate();

    void destroyGraphicsPipeline(GraphicsPipeline& graphicsPipeline);
    void destroyComputePipeline(ComputePipeline& computePipeline);

    Shader createShader(VkShaderStageFlagBits stage_, std::string filepath);
    void destroyShader(Shader& shader);

    void beginDynamicRenderer(
        const CommandBuffer& commandBuffer, 
        VkImageView imageView, VkImageLayout imageLayout,
        VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp,
        VkClearColorValue clearColor = {{ 0.0f, 0.0f, 0.0f, 1.0f }}
    );
    void endDynamicRenderer(const CommandBuffer& commandBuffer);

    VkExtent2D& getExtent() { return swapchain.getExtent(); };
    const VkExtent2D& getExtent() const { return swapchain.getExtent(); };
    Window& getWindow() { return window; }
    const Window& getWindow() const { return window; }
    uint32_t getFrame() { return currentFrame; }
    bool shouldTerminate();

    Image& getSwapchainImage(size_t imageIndex);
    ImageView& getSwapchainImageView(size_t imageIndex);

    Buffer createReadbackBuffer(size_t size, bool persistentMapping = true, std::string name = {});
    void fillBuffer(Buffer& buffer, void* data);
    void readBuffer(Buffer& buffer, void* out, size_t bytes, size_t offset = 0);
    void destroyBuffer(Buffer& buffer);

    Image createImageFromPath(std::string path, std::string name = {});

    std::string dumpGraph() const;


private:
    Window window;

    Instance instance;
    DebugMessenger debugMessenger;

    Surface surface;

    PhysicalDevice physicalDevice;
    LogicalDevice device;
    Queue graphicsQueue, presentQueue, transferQueue;

    Swapchain swapchain;
    uint64_t swapchainGeneration = 0;
    DynamicRenderer dynamicRenderer;

    DescriptorPool uiDescriptorPool;

    CommandPool graphicsCommandPool, transferCommandPool;

    uint32_t currentFrame = 0, imageIndex;

    bool framebufferResized = false;

    uint32_t currentRetirementFrame = 0;
    std::array<std::vector<std::function<void(const LogicalDevice&)>>, MAX_FRAME_IN_FLIGHT> retirementQueue;

    void flushAll();

    void initDescriptorSetLayout(DescriptorSetLayout& descriptorSetLayout);
    void destroyDescriptorSetLayout(DescriptorSetLayout& descriptorSetLayout);

    DescriptorSetGroup createDescriptorSetGroup(DescriptorSetLayout& layout, std::string name = {});
    void updateDescriptorSetGroup(DescriptorSetGroup& group, uint32_t frameIndex, const DescriptorWriter& writer);
    void destroyDescriptorSetGroup(DescriptorSetGroup& group);

    Buffer createBuffer(VkBufferUsageFlags usage, size_t size, void* data = nullptr, std::string name = {});
    Buffer createMappedBuffer(VkBufferUsageFlags usage, size_t size, std::string name = {});

    Image createImage(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, std::string name = {});
    void destroyImage(Image& image);
    ImageView createImageView(Image& image, std::string name = {});
    void destroyImageView(ImageView& imageView);
    Sampler createSampler(std::string name = {});
    void destroySampler(Sampler& sampler);

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
    void reloadComputePipeline(
        ComputePipeline& pipeline,
        const std::string& shaderPath,
        const std::vector<const DescriptorSetLayout*>& setLayouts
    );
    void reloadGraphicsPipeline(
        GraphicsPipeline& pipeline,
        VkPipelineVertexInputStateCreateInfo vertexInputInfo,
        const std::vector<std::pair<VkShaderStageFlagBits, std::string>>& shaderPaths,
        const std::vector<const DescriptorSetLayout*>& setLayouts
    );

    // Graph state
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

    std::unordered_map<uint32_t, DescriptorSetLayout> passDescriptorLayouts;
    std::unordered_map<uint32_t, DescriptorSetGroup>  passDescriptorGroups;
    std::unordered_set<uint32_t>                      dirtyDescriptors;

    std::vector<Semaphore>                  imageAvailableSemaphores;
    std::vector<std::vector<Semaphore>>     groupSemaphores;
    std::vector<std::vector<Fence>>         groupFences;
    std::vector<std::vector<CommandBuffer>> groupCommandBuffers;

    void initImages();
    void destroyImages();

    void initBuffers();
    void destroyBuffers();
    
    void initPerFrameBuffers();
    void destroyPerFrameBuffers();
    
    void initDescriptorSets();
    void destroyDescriptorSets();
    void updateDescriptors(const PassHandle& pass, uint32_t frameIndex);
    void refreshDescriptorsForImage(uint32_t imageId, uint32_t frameIndex);
    void refreshDescriptorsForBuffer(uint32_t bufferId);
    
    void initPipelines();
    void destroyPipelines();
    
    void initSync();
    void destroySync();

    void emitBarrier(const CommandBuffer& commandBuffer, const PassImageBarrier& barrier) const;

    void initImgui();
    void recreateSwapchain();

    std::vector<const char*> getRequiredExtensions();
    int scoreDevice(const PhysicalDevice& device);
};
