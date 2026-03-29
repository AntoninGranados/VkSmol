#pragma once

#include <vector>

#include "./descriptor_pool.hpp"
#include "./descriptor_set.hpp"

class LogicalDevice;

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
    DescriptorSet& current(uint32_t frameIndex);
    bool isValide() const { return pool.get() != VK_NULL_HANDLE; }
};
