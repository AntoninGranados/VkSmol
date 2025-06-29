#pragma once

#include "./instance.hpp"
#include "./debugMessenger.hpp"
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
    PhysicalDevice physicalDevice;
    LogicalDevice device;
    Queue graphicsQueue;

    void initWindow();
    void initVulkan();
    std::vector<const char*> getRequiredExtensions();

    void mainLoop();

    void cleanup();
};
