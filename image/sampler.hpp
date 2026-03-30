#pragma once

#include <vulkan/vulkan.h>

#include "../unique_handle.hpp"
#include "../tracked_resource.hpp"

class LogicalDevice;
class PhysicalDevice;

class Sampler: public UniqueHandle<VkSampler, VK_NULL_HANDLE>, public TrackedResource<ResourceType::Sampler> {
public:
    void init(const LogicalDevice& device, PhysicalDevice physicalDevice);
    void destroy(const LogicalDevice& device);
};
