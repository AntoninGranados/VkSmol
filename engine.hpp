#pragma once

#include <vulkan/vulkan.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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
#include "./render/pipeline/vertex_input.hpp"

#include "./command/command_pool.hpp"
#include "./command/command_buffer.hpp"

#include "./memory/buffer.hpp"
#include "./memory/per_frame_buffer.hpp"
#include "./memory/dynamic_per_frame_buffer.hpp"
#include "./memory/shared_buffer.hpp"

#include "./sync/semaphore.hpp"
#include "./sync/fence.hpp"

#include "./image/image.hpp"
#include "./image/image_view.hpp"
#include "./image/sampler.hpp"
#include "./image/image_barrier.hpp"

#include "./resource/resource_registry.hpp"
#include "./resource/resource_manager.hpp"

#include "./frame_context.hpp"

// TODO: create a specific command pool for one time use command buffer (like in the buffer copy function) and use VK_COMMAND_POOL_CREATE_TRANSIENT_BIT during creation
// TODO: allow the use of a single buffer for better caching and use the offset parameters
// TODO: use more assert

class VkSmol {
/*
    VkSmol engine;
    engine.init("XXXXXX", VK_MAKE_API_VERSION(V, M, m, P));

    CommandBuffer cmdBuff;
    while (!enging.shouldTerminate()) {
        beginFrame();

        cmdBuff = beginRecordingRender();
        {
            draw(cmdBuff);
        }
        endRecoringRender(cmdBuff);

        cmdBuff = beginRecordingUiRender();
        {
            drawUi(cmdBuff);
        }
        endRecoringUiRender(cmdBuff);

        endFrame();
    }
    engine.terminate();
*/

public:
    void init(std::string appName, uint32_t appVersion);

    std::optional<FrameContext> beginFrame();
    void endFrame();

    CommandBuffer& beginRecordingRender();
    void endRecoringRender(CommandBuffer& commandBuffer);
    
    CommandBuffer& beginRecordingUiRender();
    void endRecoringUiRender(CommandBuffer& commandBuffer);

    void waitIdle();
    void terminate();

    GraphicsPipeline initGraphicsPipeline(
        VkPipelineVertexInputStateCreateInfo vertexInputInfo,
        const std::vector<const Shader*>& shaders,
        const std::vector<const DescriptorSetLayout*>& setLayouts,
        GraphicsPipeline other = GraphicsPipeline(),
        VkFormat colorFormat = VK_FORMAT_UNDEFINED
    );
    void destroyGraphicsPipeline(GraphicsPipeline& graphicsPipeline);

    Shader createShader(VkShaderStageFlagBits stage_, std::string filepath);
    void destroyShader(Shader& shader);

    void beginDynamicRenderer(
        const CommandBuffer& commandBuffer, 
        VkImageView imageView, VkImageLayout imageLayout,
        VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp,
        VkClearColorValue clearColor = {{ 0.0f, 0.0f, 0.0f, 1.0f }}
    );
    void endDynamicRenderer(const CommandBuffer& commandBuffer);

    void barrier(
        const CommandBuffer& commandBuffer,
        VkImage image,
        VkImageLayout srcLayout, VkImageLayout dstLayout,
        VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
        VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask
    );
    
    VkExtent2D& getExtent() { return swapchain.getExtent(); };
    const VkExtent2D& getExtent() const { return swapchain.getExtent(); };
    Window& getWindow() { return window; }
    const Window& getWindow() const { return window; }
    uint32_t getFrame() { return currentFrame; }
    bool shouldTerminate();

    Image& getSwapchainImage(size_t imageIndex);
    ImageView& getSwapchainImageView(size_t imageIndex);

    void initDescriptorSetLayout(DescriptorSetLayout& descriptorSetLayout);
    void destroyDescriptorSetLayout(DescriptorSetLayout& descriptorSetLayout);

    DescriptorSetGroup createDescriptorSetGroup(DescriptorSetLayout& layout, std::string name = {});
    void updateDescriptorSetGroup(DescriptorSetGroup& group, uint32_t frameIndex, const DescriptorWriter& writer);
    void destroyDescriptorSetGroup(DescriptorSetGroup& group);

    Buffer createBuffer(VkBufferUsageFlags usage, size_t size, void* data = nullptr, std::string name = {});
    Buffer createReadbackBuffer(size_t size, bool persistentMapping = true, std::string name = {});
    void fillBuffer(Buffer& buffer, void* data);
    void readBuffer(Buffer& buffer, void* out, size_t bytes, size_t offset = 0);
    void destroyBuffer(Buffer& buffer);

    Image createImage(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, std::string name = {});
    Image createImageFromPath(std::string path, std::string name = {});
    void destroyImage(Image& image);
    ImageView createImageView(Image& image, std::string name = {});
    void destroyImageView(ImageView& imageView);
    Sampler createSampler(std::string name = {});
    void destroySampler(Sampler& sampler);
    
    template<typename T>
    PerFrameBuffer<T> createPerFrameBuffer(VkBufferUsageFlags usage, size_t count = 1, std::string name = {}) {
        return resourceManager.createPerFrameBuffer<T>(usage, count, name);
    }
    template<typename T>
    void destroyPerFrameBuffer(PerFrameBuffer<T>& buffer) {
        resourceManager.destroyPerFrameBuffer<T>(buffer);
    }
    
    template<typename T>
    SharedBuffer<T> createSharedBuffer(VkBufferUsageFlags usage, size_t count = 1, std::string name = {}) {
        return resourceManager.createSharedBuffer<T>(usage, count, name);
    }
    template<typename T>
    void destroySharedBuffer(SharedBuffer<T>& buffer) {
        resourceManager.destroySharedBuffer<T>(buffer);
    }
    
    template <typename T, typename Header = std::monostate>
    DynamicPerFrameBuffer<T, Header> createDynamicPerFrameBuffer(VkBufferUsageFlags usage, size_t count = 0, std::string name = {}) {
        return resourceManager.createDynamicPerFrameBuffer<T, Header>(usage, count, name);
    }
    template <typename T, typename Header = std::monostate>
    bool setDynamicPerFrameBufferCount(DynamicPerFrameBuffer<T, Header>& buffer, size_t count) {
        return resourceManager.setDynamicPerFrameBufferCount<T, Header>(buffer, count);
    }
    template <typename T, typename Header = std::monostate>
    void clearDynamicPerFrameBuffer(DynamicPerFrameBuffer<T, Header>& buffer) {
        resourceManager.clearDynamicPerFrameBuffer<T, Header>(buffer);
    }
    template <typename T, typename Header = std::monostate>
    void destroyDynamicPerFrameBuffer(DynamicPerFrameBuffer<T, Header>& buffer) {
        resourceManager.destroyDynamicPerFrameBuffer<T, Header>(buffer);
    }

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

    CommandPool graphicsCommandPool, transferCommandPool, uiCommandPool;
    std::vector<CommandBuffer> commandBuffers, uiCommandBuffers;

    std::vector<Semaphore> imageAvailableSemaphores;
    std::vector<std::vector<Semaphore> > renderFinishedSemaphores, uiFinishedSemaphores;    // One semaphore per image in the swapchain
    std::vector<Fence> renderInFlightFences, uiInFlightFences;
    uint32_t currentFrame = 0, imageIndex;

    bool framebufferResized = false;

    ResourceManager resourceManager;

    void initImgui();

    void recreatePerFrameSynchObjects();
    void recreateSwapchain();
    
    std::vector<const char*> getRequiredExtensions();
    int scoreDevice(const PhysicalDevice& device);
};
