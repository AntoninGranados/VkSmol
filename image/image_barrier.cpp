#include "image_barrier.hpp"

#include "../command/command_buffer.hpp"

namespace VkSmol {

void ImageBarrier::call(
    const CommandBuffer& commandBuffer,
    VkImage image,
    VkImageLayout srcLayout, VkImageLayout dstLayout,
    VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
    VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask,
    VkImageAspectFlags aspect
) {
    barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = srcLayout;
    barrier.newLayout = dstLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspect;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = srcAccessMask;
    barrier.dstAccessMask = dstAccessMask;

    vkCmdPipelineBarrier(
        commandBuffer.get(),
        srcStageMask, dstStageMask,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
}

}
