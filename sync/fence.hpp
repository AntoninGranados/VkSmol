#pragma once

#include <vulkan/vulkan.h>

#include <vector>

#include "../base/unique_handle.hpp"

namespace VkSmol {

class LogicalDevice;

class Fence: public UniqueHandle<VkFence, VK_NULL_HANDLE> {
public:
    // The fence is not signaled by default
    void init(const LogicalDevice& device, bool signaled = false);
    void destroy(const LogicalDevice& device);
    
    static void wait(const std::vector<const Fence*>& fences, const LogicalDevice& device, uint64_t timeout = UINT64_MAX);
    void wait(const LogicalDevice& device, uint64_t timeout = UINT64_MAX);
    static void reset(const std::vector<const Fence*>& fences, const LogicalDevice& device);
    void reset(const LogicalDevice& device);
};

}
