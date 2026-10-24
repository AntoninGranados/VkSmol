#pragma once

#include <vulkan/vulkan.h>

#include <vector>

#include "../../base/tracked_resource.hpp"

namespace VkSmol {

class LogicalDevice;
class CommandBuffer;
class DescriptorSetLayout;
class DescriptorPool;
class DescriptorSetGroup;

class DescriptorSet: public TrackedResource {
public:
    void alloc(const LogicalDevice& device, const DescriptorPool& descriptorPool, const DescriptorSetLayout& descriptorSetLayout);
    static void free(const LogicalDevice& device, DescriptorSetGroup& descriptorSetGroup);
    VkDescriptorSet get() { return descriptorSet; };

    void linkUniformBuffer(VkBuffer buffer, VkDeviceSize size, uint32_t binding);
    void linkStorageBuffer(VkBuffer buffer, VkDeviceSize size, uint32_t binding);
    void linkCombinedImageSampler(VkImageView view, VkSampler sampler, uint32_t binding);
    void linkStorageImage(VkImageView view, uint32_t binding);
    void update(const LogicalDevice& device);
    void bind(const CommandBuffer& commandBuffer, VkPipelineLayout pipelineLayout, VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS, uint32_t setIndex = 0);
    void clear();

private:
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    
    std::vector<std::pair<VkDescriptorBufferInfo, uint32_t> > bufferInfos;
    std::vector<std::pair<VkDescriptorBufferInfo, uint32_t> > storageInfos;
    std::vector<std::pair<VkDescriptorImageInfo, uint32_t> > imageInfos;
    std::vector<std::pair<VkDescriptorImageInfo, uint32_t> > storageImageInfos;
    std::vector<VkWriteDescriptorSet> descriptorWrites;
};

}
