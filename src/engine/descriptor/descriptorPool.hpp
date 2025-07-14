#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>

class LogicalDevice;

class DescriptorPool {
public:
    void init(LogicalDevice device);
    void destroy(LogicalDevice device);
    VkDescriptorPool get() { return m_DescriptorPool; };

    void addPoolSize(VkDescriptorType descriptorType, uint32_t descriptorCount);

private:
    VkDescriptorPool m_DescriptorPool;
    std::vector<VkDescriptorPoolSize> m_PoolSizes;
    uint32_t m_MaxSize = 0; // we will only be hable to allocate the number specified in pool sizes
};
