#pragma once

#include <vulkan/vulkan.h>

#include <vector>

#include "../base/unique_handle.hpp"

class PhysicalDevice;

class LogicalDevice: public UniqueHandle<VkDevice, VK_NULL_HANDLE> {
public:
    void init(
        PhysicalDevice physicalDevice,
        std::vector<uint32_t> queueFamilyIndices,
        std::vector<const char*> extensions,
        std::vector<const char*> validationLayers
    );
    void destroy();

    void waitIdle() const;
};
