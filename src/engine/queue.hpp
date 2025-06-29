#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <optional>
#include <vector>

class PhysicalDevice;
class LogicalDevice;

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;

    bool isComplete() { return graphicsFamily.has_value(); }
};

QueueFamilyIndices findQueueFamilies(PhysicalDevice physicalDevice);


class Queue {
public:
    void init(LogicalDevice device, uint32_t queueFamilyIndex);
    // void destroy();
    VkQueue get() { return queue; };

private:
    VkQueue queue;
};
