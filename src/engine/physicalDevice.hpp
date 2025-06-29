#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <functional>

class Instance;

class PhysicalDevice {
public:
    void init(Instance instance);
    void init(Instance instance, std::function<int(VkPhysicalDevice)> scoringFunction);
    // void destroy();  //? We don't need to destroy the physical device
    VkPhysicalDevice get() { return physicalDevice; };

private:
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

    static int scoreDeviceDefault(VkPhysicalDevice device);
};