#pragma once

#include <vector>

#include "./descriptor_pool.hpp"
#include "./descriptor_set.hpp"

class LogicalDevice;

class DescriptorSetAllocation {
public:
    DescriptorPool pool;
    std::vector<DescriptorSet> sets;

    // Move only
    DescriptorSetAllocation(const DescriptorSetAllocation&) = delete;
    DescriptorSetAllocation& operator=(const DescriptorSetAllocation&) = delete;
    DescriptorSetAllocation(DescriptorSetAllocation&&) = default;
    DescriptorSetAllocation& operator=(DescriptorSetAllocation&&) = default;

    DescriptorSetAllocation() {};
    DescriptorSetAllocation(DescriptorPool &&pool_, std::vector<DescriptorSet> &&sets_):
        pool(std::move(pool_)), sets(std::move(sets_)) {};

    void destroy(LogicalDevice device);
    DescriptorSet &current (uint32_t frameIndex);
    bool isValide() const { return pool.get() != VK_NULL_HANDLE; }
};
