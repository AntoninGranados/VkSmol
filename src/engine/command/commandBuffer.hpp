#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>

class LogicalDevice;
class CommandPool;

class CommandBuffer {
public:
    void alloc(LogicalDevice device, CommandPool pool);
    void free(LogicalDevice device, CommandPool pool);
    // void free(LogicalDevice device);
    VkCommandBuffer get() { return commandBuffer; };

    void beginRecording(VkCommandBufferUsageFlagBits flags = (VkCommandBufferUsageFlagBits)0);
    void endRecording();
    void reset();

private:
    VkCommandBuffer commandBuffer;
};