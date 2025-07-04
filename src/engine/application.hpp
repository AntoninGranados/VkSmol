#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

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
#include "./pipeline/graphicsPipeline.hpp"
#include "./command/commandPool.hpp"
#include "./command/commandBuffer.hpp"

#include "./sync/semaphore.hpp"
#include "./sync/fence.hpp"

#include "./utils.hpp"

// TODO: remove error checking (everywhere) when using NDEBUG

class Application {
public:
    void run();

private:
    GLFWwindow* window;

    Instance instance;
    DebugMessenger debugMessenger;

    Surface surface;
    
    PhysicalDevice physicalDevice;
    LogicalDevice device;
    Queue graphicsQueue, presentQueue;

    Swapchain swapchain;
    Framebuffer framebuffer;

    RenderPass renderPass;
    GraphicsPipeline graphicsPipeline;

    CommandPool commandPool;
    CommandBuffer commandBuffer;

    Semaphore imageAvailableSemaphore, renderFinishedSemaphore;
    Fence inFlightFence;

    void initWindow();
    void initVulkan();

    std::vector<const char*> getRequiredExtensions();
    int scoreDevice(PhysicalDevice device);

    void recordCommandBuffer(CommandBuffer commandBuffer, uint32_t imageIndex);

    void mainLoop();
    void drawFrame();

    void cleanup();
};
