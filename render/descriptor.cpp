#include "descriptor.hpp"

#include <sstream>
#include <stdexcept>

#include "../command/command_buffer.hpp"
#include "../device/logical_device.hpp"
#include "../utils.hpp"

namespace VkSmol {

DescriptorPool::DescriptorPool(DescriptorPool&& other) noexcept
    : descriptorPool(other.descriptorPool),
    poolSizes(std::move(other.poolSizes)) {
    other.descriptorPool = VK_NULL_HANDLE;
}

DescriptorPool& DescriptorPool::operator=(DescriptorPool&& other) noexcept {
    if (this == &other) return *this;

    descriptorPool = other.descriptorPool;
    poolSizes = std::move(other.poolSizes);

    other.descriptorPool = VK_NULL_HANDLE;

    return *this;
}

void DescriptorPool::init(const LogicalDevice& device, uint32_t maxSets) {
    VkDescriptorPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    createInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    createInfo.pPoolSizes = poolSizes.data();
    createInfo.maxSets = maxSets;
    createInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    vkCheck(vkCreateDescriptorPool(device.get(), &createInfo, nullptr, &descriptorPool), "Failed to create descriptor pool");
}

void DescriptorPool::destroy(const LogicalDevice& device) {
    poolSizes.clear();
    vkDestroyDescriptorPool(device.get(), descriptorPool, nullptr);
    descriptorPool = VK_NULL_HANDLE;
}


void DescriptorPool::addPoolSize(VkDescriptorType descriptorType, uint32_t descriptorCount) {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = descriptorType;
    poolSize.descriptorCount = descriptorCount;

    poolSizes.emplace_back(poolSize);
}

void DescriptorSetLayout::init(const LogicalDevice& device) {
    VkDescriptorSetLayoutCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    createInfo.pBindings = bindings.data();

    vkCheck(vkCreateDescriptorSetLayout(device.get(), &createInfo, nullptr, &rawHandle()), "Failed to create descriptor set layout");
}

void DescriptorSetLayout::destroy(const LogicalDevice& device) {
    vkDestroyDescriptorSetLayout(device.get(), rawHandle(), nullptr);
}

void DescriptorSetLayout::addBinding(VkShaderStageFlags shaderStage, VkDescriptorType descriptorType, uint32_t descriptorCount) {
    VkDescriptorSetLayoutBinding layoutBinding{};
    layoutBinding.binding = static_cast<uint32_t>(bindings.size());
    layoutBinding.stageFlags = shaderStage;
    layoutBinding.descriptorType = descriptorType;
    layoutBinding.descriptorCount = descriptorCount;
    bindings.push_back(layoutBinding);
}

void DescriptorSetLayout::addBinding(uint32_t bindingIndex, VkShaderStageFlags shaderStage, VkDescriptorType descriptorType, uint32_t descriptorCount) {
    VkDescriptorSetLayoutBinding layoutBinding{};
    layoutBinding.binding = bindingIndex;
    layoutBinding.stageFlags = shaderStage;
    layoutBinding.descriptorType = descriptorType;
    layoutBinding.descriptorCount = descriptorCount;
    bindings.push_back(layoutBinding);
}

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

void DescriptorSetGroup::destroy(const LogicalDevice& device) {
    DescriptorSet::free(device, *this);
    pool.destroy(device);
}

DescriptorSet& DescriptorSetGroup::at(uint32_t frameIndex) {
    if (frameIndex >= sets.size()) {
        std::ostringstream message;
        message << "DescriptorSetGroup: frame index " << frameIndex
                << " is out of range for " << sets.size() << " descriptor sets";
        throw std::out_of_range(message.str());
    }
    return sets[frameIndex];
}

}
