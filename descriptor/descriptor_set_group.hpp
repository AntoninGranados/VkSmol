#pragma once

#include <vector>

#include "./descriptor_pool.hpp"
#include "./descriptor_set.hpp"

#include "../tracked_resource.hpp"

class LogicalDevice;

class DescriptorSetGroup: public TrackedResource<ResourceType::DescriptorSetGroup> {
public:
    DescriptorPool pool;
    std::vector<DescriptorSet> sets;
    std::vector<std::vector<ResourceId>> frameDependencies;

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
    void initDependencyTracking(size_t frameCount);
    const std::vector<ResourceId>& currentDependencies(uint32_t frameIndex) const;
    void replaceDependencies(uint32_t frameIndex, std::vector<ResourceId> dependencies);
    std::vector<ResourceId> getAllDependencies() const;
    bool isValide() const { return pool.get() != VK_NULL_HANDLE; }
};
