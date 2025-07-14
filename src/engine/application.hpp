#pragma once

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_vulkan.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>

#include "./window.hpp"

#include "./instance.hpp"
#include "./debugMessenger.hpp"
#include "./surface.hpp"

#include "./physicalDevice.hpp"
#include "./logicalDevice.hpp"
#include "./queue.hpp"

#include "./swapchain.hpp"
#include "./framebuffer.hpp"

#include "./shader.hpp"
#include "./renderPass.hpp"
#include "./descriptor/descriptorSetLayout.hpp"
#include "./descriptor/descriptorPool.hpp"
#include "./descriptor/descriptorSet.hpp"
#include "./pipeline/graphicsPipeline.hpp"
#include "./pipeline/vertexInput.hpp"

#include "./command/commandPool.hpp"
#include "./command/commandBuffer.hpp"

#include "./memory/buffer.hpp"

#include "./sync/semaphore.hpp"
#include "./sync/fence.hpp"

#include "./utils.hpp"

// TODO: remove error checking (everywhere) when using NDEBUG
// TODO: clear every vector when using destroy in a function (in case we want to recreate the object by recalling init)
// TODO: create a specific command pool for one time use command buffer (like in the buffer copy function) and use VK_COMMAND_POOL_CREATE_TRANSIENT_BIT during creation
// TODO: allow the use of a single buffer for better caching and use the offset parameters

struct Vertex {
    glm::vec2 position;
    glm::vec3 color;
};

struct UBO {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

class Application {
public:
    void run();

private:
    Window window;

    Instance instance;
    DebugMessenger debugMessenger;

    Surface surface;
    
    PhysicalDevice physicalDevice;
    LogicalDevice device;
    Queue graphicsQueue, presentQueue, transferQueue;

    Swapchain swapchain;

    RenderPass renderPass, uiRenderPass;
    DescriptorSetLayout uboLayout;
    GraphicsPipeline graphicsPipeline; //!, uiPipeline;
    
    DescriptorPool descriptorPool, uiDescriptorPool;
    std::vector<DescriptorSet> descriptorSets;

    CommandPool graphicsCommandPool, transferCommandPool, uiCommandPool;
    std::vector<CommandBuffer> commandBuffers, uiCommandBuffers;

    Buffer vertexBuffer, indexBuffer;
    std::vector<Buffer> uniformBuffers;

    std::vector<Semaphore> imageAvailableSemaphores;
    std::vector<std::vector<Semaphore> > renderFinishedSemaphores, uiFinishedSemaphores;    // One semaphore per image in the swap chain
    std::vector<Fence> renderInFlightFences, uiInFlightFences;
    uint32_t currentFrame = 0;

    bool framebufferResized = false;

    void init();
    void initImgui();

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
    std::vector<const char*> getRequiredExtensions();
    int scoreDevice(PhysicalDevice device);

    void recordCommandBuffer(CommandBuffer commandBuffer, uint32_t imageIndex);
    void recordUiCommandBuffer(CommandBuffer commandBuffer, uint32_t imageIndex);
    void updateUniformBuffer(uint32_t currentFrame);

    void mainLoop();
    void drawFrame();

    void cleanup();
};
