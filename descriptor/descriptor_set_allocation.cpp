#include "descriptor_set_allocation.hpp"

#include "../logical_device.hpp"

void DescriptorSetAllocation::destroy(LogicalDevice device) {
    DescriptorSet::free(device, *this);
    pool.destroy(device);
}

DescriptorSet &DescriptorSetAllocation::get(uint32_t frameIndex) {
    return sets[frameIndex];
}