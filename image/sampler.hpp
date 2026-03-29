#pragma once

#include <vulkan/vulkan.h>

#include "../unique_handle.hpp"

class LogicalDevice;
class PhysicalDevice;


class Sampler: public UniqueHandle<VkSampler, VK_NULL_HANDLE> {
public:
    void init(const LogicalDevice& device, PhysicalDevice physicalDevice);
    void destroy(const LogicalDevice& device);
};
