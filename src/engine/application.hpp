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
// TODO: clear every vector when using destroy in a function (in case we want to recreate the object by recalling init)

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

    RenderPass renderPass;
    GraphicsPipeline graphicsPipeline;

    CommandPool commandPool;
    std::vector<CommandBuffer> commandBuffers;

    std::vector<Semaphore> imageAvailableSemaphores;
    std::vector<std::vector<Semaphore> > renderFinishedSemaphores;    // One semaphore per image in the swap chain
    std::vector<Fence> inFlightFences;
    uint32_t currentFrame = 0;

    bool framebufferResized = false;

    void initWindow();
    void initVulkan();

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
    std::vector<const char*> getRequiredExtensions();
    int scoreDevice(PhysicalDevice device);

    void recordCommandBuffer(CommandBuffer commandBuffer, uint32_t imageIndex);

    void mainLoop();
    void drawFrame();

    void cleanup();
};
