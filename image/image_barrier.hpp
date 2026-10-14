#pragma once

#include <vulkan/vulkan.h>

namespace VkSmol {

class CommandBuffer;
class LogicalDevice;

class ImageBarrier {
public:
    void call(
        const LogicalDevice& device,
        const CommandBuffer& commandBuffer,
        VkImage image,
        VkImageLayout srcLayout, VkImageLayout dstLayout,
        VkAccessFlags2 srcAccessMask, VkAccessFlags2 dstAccessMask,
        VkPipelineStageFlags2 srcStageMask, VkPipelineStageFlags2 dstStageMask,
        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT
    );
};

}
