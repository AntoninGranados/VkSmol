#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>

#include "../base/unique_handle.hpp"

class LogicalDevice;

class DescriptorSetLayout: public UniqueHandle<VkDescriptorSetLayout, VK_NULL_HANDLE> {
public:
    void init(const LogicalDevice& device);
    void destroy(const LogicalDevice& device);
    const std::vector<VkDescriptorSetLayoutBinding>& getBindings() const { return bindings; };

    void addBinding(VkShaderStageFlags shaderStage, VkDescriptorType descriptorType, uint32_t descriptorCount = 1);

private:
    std::vector<VkDescriptorSetLayoutBinding> bindings;
};
