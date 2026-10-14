#pragma once

#include <vulkan/vulkan.h>

#include <vector>

#include "../graph/builder_resource.hpp"
#include "image_subresource.hpp"

namespace VkSmol {

struct ImageTransition {
    ImageSubresourceRange subresources;
    ImageAccessInfo from;
    ImageAccessInfo to;

    VkImageMemoryBarrier2 toVk(VkImage image) const;
};

std::vector<ImageTransition> transitionSubresources(
    std::vector<ImageAccessInfo>& accesses, const ImageSubresourceLayout& layout, const ImageSubresourceRange& range,
    const ImageAccessInfo& access
);

} // namespace VkSmol
