#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class LogicalDevice;

class Semaphore {
public:
    void init(LogicalDevice device);
    void destroy(LogicalDevice device);
    VkSemaphore get() { return semaphore; };

private:
    VkSemaphore semaphore;
};
