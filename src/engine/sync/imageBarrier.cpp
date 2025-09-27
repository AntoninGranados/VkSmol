#include "imageBarrier.hpp"

#include "../command/commandBuffer.hpp"

void ImageBarrier::init(
    VkImage image,
    VkImageLayout srcLayout, VkImageLayout dstLayout,
    VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask
) {
    barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = srcLayout;
    barrier.newLayout = dstLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = srcAccessMask;
    barrier.dstAccessMask = dstAccessMask;
}

void ImageBarrier::call(
    CommandBuffer commandBuffer,
    VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask
) {
    vkCmdPipelineBarrier(
        commandBuffer.get(),
        srcStageMask, dstStageMask,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
}
