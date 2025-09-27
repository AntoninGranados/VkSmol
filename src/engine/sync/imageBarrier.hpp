#pragma once

#include <vulkan/vulkan.h>

class CommandBuffer;

class ImageBarrier {
public:
    void init(
        VkImage image,
        VkImageLayout srcLayout, VkImageLayout dstLayout,
        VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask
    );

    void call(
        CommandBuffer commandBuffer,
        VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask
    );

    VkImageMemoryBarrier get() { return barrier; }

private:
    VkImageMemoryBarrier barrier;
};
