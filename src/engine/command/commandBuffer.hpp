#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class LogicalDevice;
class CommandPool;

class CommandBuffer {
public:
    void allocate(LogicalDevice device, CommandPool pool);
    // void free(LogicalDevice device);
    VkCommandBuffer get() { return commandBuffer; };

    void beginRecording();
    void endRecording();

private:
    VkCommandBuffer commandBuffer;
};