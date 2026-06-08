#pragma once

#include <vulkan/vulkan.h>

#include <vector>

#include "../../base/tracked_resource.hpp"

class LogicalDevice;
class Buffer;
class CommandBuffer;
class ImageView;
class Sampler;
class DescriptorSetLayout;
class DescriptorPool;
class DescriptorSetGroup;

class DescriptorSet: public TrackedResource<ResourceType::DescriptorSet> {
public:
    void alloc(const LogicalDevice& device, const DescriptorPool& descriptorPool, const DescriptorSetLayout& descriptorSetLayout);
    static void free(const LogicalDevice& device, DescriptorSetGroup& descriptorSetGroup);
    VkDescriptorSet get() { return descriptorSet; };

    void linkUniformBuffer(const Buffer& buffer, uint32_t binding);
    void linkStorageBuffer(const Buffer& buffer, uint32_t binding);
    void linkCombinedImageSampler(const ImageView& view, const Sampler& sampler, uint32_t binding);
    void linkStorageImage(const ImageView& view, uint32_t binding);
    void update(const LogicalDevice& device);
    void bind(const CommandBuffer& commandBuffer, VkPipelineLayout pipelineLayout, VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS);
    void clear();

private:
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    
    std::vector<std::pair<VkDescriptorBufferInfo, uint32_t> > bufferInfos;
    std::vector<std::pair<VkDescriptorBufferInfo, uint32_t> > storageInfos;
    std::vector<std::pair<VkDescriptorImageInfo, uint32_t> > imageInfos;
    std::vector<std::pair<VkDescriptorImageInfo, uint32_t> > storageImageInfos;
    std::vector<VkWriteDescriptorSet> descriptorWrites;
};
