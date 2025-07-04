#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class LogicalDevice;

class Fence {
public:
    // The fence is not signaled by default
    void init(LogicalDevice device, bool signaled = false);
    void destroy(LogicalDevice device);
    VkFence get() { return fence; };

    // TODO: add static versions of both functions that accepts multiple fences
    void wait(LogicalDevice device, uint64_t timeout = UINT64_MAX);
    void reset(LogicalDevice device);

private:
    VkFence fence;
};
