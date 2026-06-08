#pragma once

#include <vulkan/vulkan.h>

class CommandBuffer;

class ImageBarrier {
public:
    void call(
        const CommandBuffer& commandBuffer,
        VkImage image,
        VkImageLayout srcLayout, VkImageLayout dstLayout,
        VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
        VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask,
        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT
    );

    VkImageMemoryBarrier get() { return barrier; }

private:
    VkImageMemoryBarrier barrier;
};
