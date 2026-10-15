#pragma once

#include <vulkan/vulkan.h>

namespace VkSmol {

VkImageAspectFlags formatAspects(VkFormat format);
VkImageAspectFlags formatViewAspect(VkFormat format);

} // namespace VkSmol
