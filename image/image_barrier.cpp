#include "image_barrier.hpp"

#include "../command/command_buffer.hpp"
#include "../device/logical_device.hpp"

namespace VkSmol {

void ImageBarrier::call(
    const LogicalDevice& device,
    const CommandBuffer& commandBuffer,
    VkImage image,
    VkImageLayout srcLayout, VkImageLayout dstLayout,
    VkAccessFlags2 srcAccessMask, VkAccessFlags2 dstAccessMask,
    VkPipelineStageFlags2 srcStageMask, VkPipelineStageFlags2 dstStageMask,
    VkImageAspectFlags aspect
) {
    const VkImageMemoryBarrier2 barrier{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask        = srcStageMask,
        .srcAccessMask       = srcAccessMask,
        .dstStageMask        = dstStageMask,
        .dstAccessMask       = dstAccessMask,
        .oldLayout           = srcLayout,
        .newLayout           = dstLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = image,
        .subresourceRange    = {
            .aspectMask     = aspect,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1,
        },
    };

    const VkDependencyInfo dependency{
        .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers    = &barrier,
    };
    device.functions().cmdPipelineBarrier2(commandBuffer.get(), &dependency);
}

}
