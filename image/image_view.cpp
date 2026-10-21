#include "image_view.hpp"

#include "image.hpp"
#include "../device/logical_device.hpp"
#include "../utils.hpp"

namespace VkSmol {

void ImageView::init(const LogicalDevice& device, const Image& image) {
    init(
        device, image, VK_IMAGE_VIEW_TYPE_2D,
        {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        }
    );
}

void ImageView::init(
    const LogicalDevice& device, const Image& image, VkImageViewType type, const VkImageSubresourceRange& range
) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image.get();
    viewInfo.viewType = type;
    viewInfo.format = image.getFormat();
    viewInfo.subresourceRange = range;

    vkCheck(vkCreateImageView(device.get(), &viewInfo, nullptr, &rawHandle()), "Failed to create the image view");
}

void ImageView::destroy(const LogicalDevice& device) {
    if (isValid()) {
        vkDestroyImageView(device.get(), rawHandle(), nullptr);
    }
    resetHandle();
}

}
