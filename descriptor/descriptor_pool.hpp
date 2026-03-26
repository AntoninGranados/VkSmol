#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>

class LogicalDevice;

class DescriptorPool {
public:
    // Can't copy (onyl move)
    DescriptorPool(const DescriptorPool&) = delete;
    DescriptorPool& operator=(const DescriptorPool&) = delete;
    DescriptorPool(DescriptorPool&& other) noexcept;
    DescriptorPool& operator=(DescriptorPool&& other) noexcept;

    DescriptorPool() = default;

    void init(LogicalDevice device);
    void destroy(LogicalDevice device);
    VkDescriptorPool get() const { return descriptorPool; };

    void addPoolSize(VkDescriptorType descriptorType, uint32_t descriptorCount);

private:
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorPoolSize> poolSizes;
    uint32_t maxSize = 0; // we will only be hable to allocate the number specified in pool sizes
};
