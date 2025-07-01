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
#include "./shader.hpp"
#include "./utils.hpp"


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

    void initWindow();
    void initVulkan();
    std::vector<const char*> getRequiredExtensions();

    void createPipeline();

    int scoreDevice(PhysicalDevice device);

    void mainLoop();

    void cleanup();
};
