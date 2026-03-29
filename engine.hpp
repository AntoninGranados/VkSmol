#pragma once

#include <vulkan/vulkan.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <string>

#include "./window.hpp"

#include "./instance.hpp"
#include "./debug_messenger.hpp"
#include "./surface.hpp"

#include "./physical_device.hpp"
#include "./logical_device.hpp"
#include "./queue.hpp"

#include "./swapchain.hpp"
#include "./dynamic_renderer.hpp"

#include "./descriptor/descriptor_set_layout.hpp"
#include "./descriptor/descriptor_pool.hpp"
#include "./descriptor/descriptor_set.hpp"
#include "./descriptor/descriptor_set_group.hpp"
#include "./descriptor/descriptor_writer.hpp"

#include "./shader.hpp"
#include "./pipeline/graphics_pipeline.hpp"

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

    VkExtent2D getExtent() { return swapchain.getExtent(); };

    GraphicsPipeline initGraphicsPipeline(
        VkPipelineVertexInputStateCreateInfo vertexInputInfo,
        const std::vector<const Shader*>& shaders,
        const std::vector<const DescriptorSetLayout*>& setLayouts,
        GraphicsPipeline other = GraphicsPipeline(),
        VkFormat colorFormat = VK_FORMAT_UNDEFINED
    );
    void destroyGraphicsPipeline(GraphicsPipeline& graphicsPipeline);

    Shader initShader(VkShaderStageFlagBits stage_, std::string filepath);
    void destroyShader(Shader& shader);

    void initDescriptorSetLayout(DescriptorSetLayout& descriptorSetLayout);
    void destroyDescriptorSetLayout(DescriptorSetLayout& descriptorSetLayout);

    DescriptorSetGroup createDescriptorSetGroup(DescriptorSetLayout& layout);
    void updateDescriptorSetGroup(DescriptorSetGroup& group, uint32_t frameIndex, const DescriptorWriter& writer);
    void destroyDescriptorSetGroup(DescriptorSetGroup& group);

    Buffer createBuffer(VkBufferUsageFlags usage, size_t size, void* data = nullptr);
    Buffer createReadbackBuffer(size_t size, bool persistentMapping = true);
    void fillBuffer(Buffer& buffer, void* data);
    void readBuffer(Buffer& buffer, void* out, size_t bytes, size_t offset = 0);
    void destroyBuffer(Buffer& buffer);
    
    template<typename T>
    PerFrameBuffer<T> createPerFrameBuffer(VkBufferUsageFlags usage, size_t count = 1) { return PerFrameBuffer<T>::create(device, physicalDevice, usage, count); }
    template<typename T>
    void destroyPerFrameBuffer(PerFrameBuffer<T>& buffer) { buffer.destroy(device); }
    
    template<typename T>
    SharedBuffer<T> createSharedBuffer(VkBufferUsageFlags usage, size_t count = 1) { return SharedBuffer<T>::create(device, physicalDevice, usage, count); }
    template<typename T>
    void destroySharedBuffer(SharedBuffer<T>& buffer) { buffer.destroy(device); }
    
    template <typename T, typename Header = std::monostate>
    DynamicPerFrameBuffer<T, Header> createDynamicPerFrameBuffer(VkBufferUsageFlags usage, size_t count = 0) { return DynamicPerFrameBuffer<T, Header>::create(device, physicalDevice, usage, count); }
    template <typename T, typename Header = std::monostate>
    void destroyDynamicPerFrameBuffer(DynamicPerFrameBuffer<T, Header>& buffer) { buffer.destroy(device); }
    template <typename T, typename Header = std::monostate>
    bool setDynamicPerFrameBufferCount(DynamicPerFrameBuffer<T, Header>& buffer, size_t count) { return buffer.setElementCount(device, physicalDevice, count); }
    template <typename T, typename Header = std::monostate>
    void clearDynamicPerFrameBuffer(DynamicPerFrameBuffer<T, Header>& buffer) { buffer.clear(device, physicalDevice); }

    Image createImage(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags properties);
    Image createImageFromPath(std::string path);
    void destroyImage(Image& image);
    ImageView createImageView(Image& image);
    void destroyImageView(ImageView& imageView);

    Sampler createSampler();
    void destroySampler(Sampler& sampler);

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
    
    Window& getWindow() { return window; }
    const Window& getWindow() const { return window; }
    uint32_t getFrame() { return currentFrame; }
    bool shouldTerminate();

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

    void initImgui();

    void recreatePerFrameSynchObjects();
    void recreateSwapchain();
    
    std::vector<const char*> getRequiredExtensions();
    int scoreDevice(const PhysicalDevice& device);
};
