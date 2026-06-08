#include "descriptor_pool.hpp"

#include "../../device/logical_device.hpp"
#include "../../utils.hpp"


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
