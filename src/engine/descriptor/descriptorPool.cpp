#include "descriptorPool.hpp"

#include "../logicalDevice.hpp"
#include "../utils.hpp"

// TODO: maybe use a DescriptorSetLayout to construct the pool sizes
void DescriptorPool::init(LogicalDevice device) {
    VkDescriptorPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    createInfo.poolSizeCount = static_cast<uint32_t>(m_PoolSizes.size());
    createInfo.pPoolSizes = m_PoolSizes.data();
    createInfo.maxSets = m_MaxSize;

    vkCheck(vkCreateDescriptorPool(device.get(), &createInfo, nullptr, &m_DescriptorPool), "Failed to create descriptor pool");
}

void DescriptorPool::destroy(LogicalDevice device) {
    m_PoolSizes.clear();
    m_MaxSize = 0;
    vkDestroyDescriptorPool(device.get(), m_DescriptorPool, nullptr);
}


void DescriptorPool::addPoolSize(VkDescriptorType descriptorType, uint32_t descriptorCount) {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = descriptorType;
    poolSize.descriptorCount = descriptorCount;

    m_PoolSizes.emplace_back(poolSize);
    m_MaxSize += descriptorCount;
}
