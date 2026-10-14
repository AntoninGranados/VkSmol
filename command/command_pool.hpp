#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "../base/unique_handle.hpp"

namespace VkSmol {

class LogicalDevice;

class CommandPool: public UniqueHandle<VkCommandPool, VK_NULL_HANDLE> {
public:
    void init(const LogicalDevice& device, uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    void destroy(const LogicalDevice& device);
};

}
