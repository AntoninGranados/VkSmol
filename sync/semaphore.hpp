#pragma once

#include <vulkan/vulkan.h>

#include "../unique_handle.hpp"

class LogicalDevice;

class Semaphore: public UniqueHandle<VkSemaphore, VK_NULL_HANDLE> {
public:
    void init(const LogicalDevice& device);
    void destroy(const LogicalDevice& device);
};
