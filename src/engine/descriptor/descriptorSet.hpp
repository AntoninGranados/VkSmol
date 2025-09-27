#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>

class LogicalDevice;
class Buffer;
class CommandBuffer;
class DescriptorSetLayout;
class DescriptorPool;

class DescriptorSet {
public:
    void alloc(LogicalDevice device, DescriptorPool descriptorPool, DescriptorSetLayout descriptorSetLayout);
    // TODO: FIX IT
    static void alloc(
        std::vector<DescriptorSet>& descriptorSets,
        LogicalDevice device,
        DescriptorPool descriptorPool,
        std::vector<DescriptorSetLayout> descriptorSetLayouts
    );
    // void free(LogicalDevice device);
    VkDescriptorSet get() { return descriptorSet; };

    void linkBuffer(LogicalDevice device, Buffer buffer);
    void bind(CommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS);

private:
    VkDescriptorSet descriptorSet;
};
