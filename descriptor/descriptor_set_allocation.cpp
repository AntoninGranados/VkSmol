#include "descriptor_set_allocation.hpp"

#include "../logical_device.hpp"

void DescriptorSetAllocation::destroy(LogicalDevice device) {
    DescriptorSet::free(device, *this);
    pool.destroy(device);
}

DescriptorSet &DescriptorSetAllocation::current(uint32_t frameIndex) {
    return sets[frameIndex];
}