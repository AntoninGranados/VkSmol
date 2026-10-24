#pragma once

#include <vulkan/vulkan.h>

#include <utility>
#include <vector>

#include "../base/unique_handle.hpp"

namespace VkSmol {

class LogicalDevice;
class CommandBuffer;
class DescriptorSetGroup;

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

class DescriptorSet {
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

class DescriptorSetGroup {
public:
    DescriptorPool pool;
    std::vector<DescriptorSet> sets;

    // Move only
    DescriptorSetGroup(const DescriptorSetGroup&) = delete;
    DescriptorSetGroup& operator=(const DescriptorSetGroup&) = delete;
    DescriptorSetGroup(DescriptorSetGroup&&) = default;
    DescriptorSetGroup& operator=(DescriptorSetGroup&&) = default;

    DescriptorSetGroup() {};
    DescriptorSetGroup(DescriptorPool&& pool_, std::vector<DescriptorSet>&& sets_):
        pool(std::move(pool_)), sets(std::move(sets_)) {};

    void destroy(const LogicalDevice& device);
    DescriptorSet& at(uint32_t frameIndex);

    bool isValid() const { return pool.get() != VK_NULL_HANDLE; }
};

}
