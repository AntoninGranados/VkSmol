#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>

class PhysicalDevice;

class LogicalDevice {
public:
    void init(
        PhysicalDevice physicalDevice,
        uint32_t queueFamilyIndex,
        std::vector<const char*> extensions,
        std::vector<const char*> validationLayers
    );
    void destroy();
    VkDevice get() { return device; };

private:
    VkDevice device;
};
