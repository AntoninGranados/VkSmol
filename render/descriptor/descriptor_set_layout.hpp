#pragma once

#include <vulkan/vulkan.h>

#include <vector>

#include "../../base/unique_handle.hpp"

namespace VkSmol {

class LogicalDevice;

class DescriptorSetLayout: public UniqueHandle<VkDescriptorSetLayout, VK_NULL_HANDLE> {
public:
    void init(const LogicalDevice& device);
    void destroy(const LogicalDevice& device);
    const std::vector<VkDescriptorSetLayoutBinding>& getBindings() const { return bindings; };

    void addBinding(VkShaderStageFlags shaderStage, VkDescriptorType descriptorType, uint32_t descriptorCount = 1);
    void addBinding(uint32_t bindingIndex, VkShaderStageFlags shaderStage, VkDescriptorType descriptorType, uint32_t descriptorCount = 1);

private:
    std::vector<VkDescriptorSetLayoutBinding> bindings;
};

}
