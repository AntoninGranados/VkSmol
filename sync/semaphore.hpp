#pragma once

#include <vulkan/vulkan.h>

#include "../base/unique_handle.hpp"

namespace VkSmol {

class LogicalDevice;

class Semaphore: public UniqueHandle<VkSemaphore, VK_NULL_HANDLE> {
public:
    void init(const LogicalDevice& device);
    void destroy(const LogicalDevice& device);
};

}
