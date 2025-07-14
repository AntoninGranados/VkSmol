#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>

class LogicalDevice;

class DescriptorSetLayout {
public:
    void init(LogicalDevice device);
    void destroy(LogicalDevice device);
    VkDescriptorSetLayout get() { return m_DescritorSetLayout; };

    void addBinding(VkShaderStageFlags shaderStage, VkDescriptorType descriptorType, uint32_t descriptorCount = 1);

private:
    VkDescriptorSetLayout m_DescritorSetLayout;
    std::vector<VkDescriptorSetLayoutBinding> m_Bindings;
};
