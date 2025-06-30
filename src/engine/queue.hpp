#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <optional>
#include <vector>

class Surface;
class PhysicalDevice;
class LogicalDevice;

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily; // queue for graphics
    std::optional<uint32_t> presentFamily;  // queue for prensenting surface to the screen

    bool isComplete() { return graphicsFamily.has_value() && presentFamily.has_value(); }
    std::vector<uint32_t> getIndices() {
        return std::vector<uint32_t> { graphicsFamily.value(), presentFamily.value() };
    }
};

QueueFamilyIndices findQueueFamilies(PhysicalDevice physicalDevice, Surface surface);


class Queue {
public:
    void init(LogicalDevice device, uint32_t queueFamilyIndex);
    // void destroy();
    VkQueue get() { return queue; };

private:
    VkQueue queue;
};
