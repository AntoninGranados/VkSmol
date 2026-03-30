#include "descriptor_set_group.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>

#include "../logical_device.hpp"

void DescriptorSetGroup::destroy(const LogicalDevice& device) {
    DescriptorSet::free(device, *this);
    pool.destroy(device);
}

DescriptorSet& DescriptorSetGroup::current(uint32_t frameIndex) {
    if (frameIndex >= sets.size()) {
        std::ostringstream message;
        message << "DescriptorSetGroup: frame index " << frameIndex
                << " is out of range for " << sets.size() << " descriptor sets";
        throw std::out_of_range(message.str());
    }
    return sets[frameIndex];
}

void DescriptorSetGroup::initDependencyTracking(size_t frameCount) {
    frameDependencies.clear();
    frameDependencies.resize(frameCount);
}

const std::vector<ResourceId>& DescriptorSetGroup::currentDependencies(uint32_t frameIndex) const {
    if (frameIndex >= frameDependencies.size()) {
        std::ostringstream message;
        message << "DescriptorSetGroup: dependency frame index " << frameIndex
                << " is out of range for " << frameDependencies.size() << " tracked frames";
        throw std::out_of_range(message.str());
    }
    return frameDependencies[frameIndex];
}

void DescriptorSetGroup::replaceDependencies(uint32_t frameIndex, std::vector<ResourceId> dependencies) {
    if (frameIndex >= frameDependencies.size()) {
        std::ostringstream message;
        message << "DescriptorSetGroup: dependency frame index " << frameIndex
                << " is out of range for " << frameDependencies.size() << " tracked frames";
        throw std::out_of_range(message.str());
    }
    frameDependencies[frameIndex] = std::move(dependencies);
}

std::vector<ResourceId> DescriptorSetGroup::getAllDependencies() const {
    std::vector<ResourceId> dependencies;

    for (const std::vector<ResourceId>& frame : frameDependencies) {
        for (ResourceId dependency : frame) {
            if (std::find(dependencies.begin(), dependencies.end(), dependency) == dependencies.end()) {
                dependencies.push_back(dependency);
            }
        }
    }

    return dependencies;
}
