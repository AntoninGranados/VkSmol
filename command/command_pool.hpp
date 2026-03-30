#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "../base/unique_handle.hpp"

class LogicalDevice;

class CommandPool: public UniqueHandle<VkCommandPool, VK_NULL_HANDLE> {
public:
    void init(const LogicalDevice& device, uint32_t queueFamilyIndex);
    void destroy(const LogicalDevice& device);
};
