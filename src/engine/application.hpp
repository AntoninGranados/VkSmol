#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "./instance.hpp"
#include "./debugMessenger.hpp"
#include "./surface.hpp"
#include "./physicalDevice.hpp"
#include "./queue.hpp"
#include "./logicalDevice.hpp"
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

    void initWindow();
    void initVulkan();
    std::vector<const char*> getRequiredExtensions();

    int scoreDevice(PhysicalDevice device);

    void mainLoop();

    void cleanup();
};
