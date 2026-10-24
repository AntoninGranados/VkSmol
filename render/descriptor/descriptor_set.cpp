#include "descriptor_set.hpp"

#include "./descriptor_set_layout.hpp"
#include "./descriptor_pool.hpp"
#include "./descriptor_set_group.hpp"

#include "../../device/logical_device.hpp"
#include "../../command/command_buffer.hpp"
#include "../../utils.hpp"

namespace VkSmol {

void DescriptorSet::alloc(const LogicalDevice& device, const DescriptorPool& descriptorPool, const DescriptorSetLayout& descriptorSetLayout) {
    VkDescriptorSetLayout vkDescriptorSetLayout = descriptorSetLayout.get();

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool.get();
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &vkDescriptorSetLayout;

    vkCheck(vkAllocateDescriptorSets(device.get(), &allocInfo, &descriptorSet), "Failed to allocate descriptor set");
}


void DescriptorSet::free(const LogicalDevice& device, DescriptorSetGroup& allocation) {
    if (allocation.pool.get() == VK_NULL_HANDLE || allocation.sets.empty()) return;

    std::vector<VkDescriptorSet> sets(static_cast<size_t>(allocation.sets.size()));
    for (size_t i = 0; i < allocation.sets.size(); i++) {
        sets[i] = allocation.sets[i].get();
    }

    vkFreeDescriptorSets(
        device.get(),
        allocation.pool.get(),
        sets.size(),
        sets.data()
    );

    allocation.sets.clear();
}

void DescriptorSet::linkUniformBuffer(VkBuffer buffer, VkDeviceSize size, uint32_t binding) {
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = size;
    bufferInfos.push_back({ bufferInfo, binding });
}

void DescriptorSet::linkStorageBuffer(VkBuffer buffer, VkDeviceSize size, uint32_t binding) {
    VkDescriptorBufferInfo storageInfo{};
    storageInfo.buffer = buffer;
    storageInfo.offset = 0;
    storageInfo.range = size;
    storageInfos.push_back({ storageInfo, binding });
}

void DescriptorSet::linkStorageImage(VkImageView view, uint32_t binding) {
    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler = VK_NULL_HANDLE;
    imageInfo.imageView = view;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    storageImageInfos.push_back({ imageInfo, binding });
}

void DescriptorSet::linkCombinedImageSampler(VkImageView view, VkSampler sampler, uint32_t binding) {
    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler = sampler;
    imageInfo.imageView = view;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfos.push_back({ imageInfo, binding });
}

void DescriptorSet::update(const LogicalDevice& device) {
    descriptorWrites.clear();

    for (const std::pair<VkDescriptorBufferInfo, uint32_t>& pair : bufferInfos) {
            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = descriptorSet;
            descriptorWrite.dstBinding = pair.second;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = &pair.first;

            descriptorWrites.push_back(descriptorWrite);
    }
    for (const std::pair<VkDescriptorBufferInfo, uint32_t>& pair : storageInfos) {
            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = descriptorSet;
            descriptorWrite.dstBinding = pair.second;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = &pair.first;

            descriptorWrites.push_back(descriptorWrite);
    }
    for (const std::pair<VkDescriptorImageInfo, uint32_t>& pair : imageInfos) {
            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = descriptorSet;
            descriptorWrite.dstBinding = pair.second;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pImageInfo = &pair.first;

            descriptorWrites.push_back(descriptorWrite);
    }
    for (const std::pair<VkDescriptorImageInfo, uint32_t>& pair : storageImageInfos) {
            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = descriptorSet;
            descriptorWrite.dstBinding = pair.second;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pImageInfo = &pair.first;

            descriptorWrites.push_back(descriptorWrite);
    }

    vkUpdateDescriptorSets(device.get(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

void DescriptorSet::bind(const CommandBuffer& commandBuffer, VkPipelineLayout pipelineLayout, VkPipelineBindPoint bindPoint, uint32_t setIndex) {
    vkCmdBindDescriptorSets(commandBuffer.get(), bindPoint, pipelineLayout, setIndex, 1, &descriptorSet, 0, nullptr);
}

void DescriptorSet::clear() {
    bufferInfos.clear();
    storageInfos.clear();
    imageInfos.clear();
    storageImageInfos.clear();
    descriptorWrites.clear();
}

}
