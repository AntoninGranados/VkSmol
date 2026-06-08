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

    void init(const LogicalDevice& device, uint32_t maxSets);
    void destroy(const LogicalDevice& device);
    VkDescriptorPool get() const { return descriptorPool; };

    void addPoolSize(VkDescriptorType descriptorType, uint32_t descriptorCount);

private:
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorPoolSize> poolSizes;
};
