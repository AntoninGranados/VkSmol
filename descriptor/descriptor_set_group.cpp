#include "descriptor_set_group.hpp"

#include <sstream>
#include <stdexcept>

#include "../logical_device.hpp"

void DescriptorSetGroup::destroy(const LogicalDevice& device) {
    DescriptorSet::free(device, *this);
    pool.destroy(device);
}

DescriptorSet& DescriptorSetGroup::at(uint32_t frameIndex) {
    if (frameIndex >= sets.size()) {
        std::ostringstream message;
        message << "DescriptorSetGroup: frame index " << frameIndex
                << " is out of range for " << sets.size() << " descriptor sets";
        throw std::out_of_range(message.str());
    }
    return sets[frameIndex];
}
