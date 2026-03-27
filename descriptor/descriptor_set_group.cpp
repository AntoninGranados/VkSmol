#include "descriptor_set_group.hpp"

#include "../logical_device.hpp"

void DescriptorSetGroup::destroy(LogicalDevice device) {
    DescriptorSet::free(device, *this);
    pool.destroy(device);
}

DescriptorSet &DescriptorSetGroup::current(uint32_t frameIndex) {
    return sets[frameIndex];
}
