#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <set>
#include <vector>

class PhysicalDevice;

class LogicalDevice {
public:
    void init(
        PhysicalDevice physicalDevice,
        std::vector<uint32_t> queueFamilyIndices,
        std::vector<const char*> extensions,
        std::vector<const char*> validationLayers
    );
    void destroy();
    VkDevice get() { return device; };

private:
    VkDevice device;
};
