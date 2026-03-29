#include "image_view.hpp"

#include "image.hpp"
#include "../logical_device.hpp"
#include "../utils.hpp"

void ImageView::init(const LogicalDevice& device, const Image& image) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image.get();
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = image.getFormat();
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    vkCheck(vkCreateImageView(device.get(), &viewInfo, nullptr, &rawHandle()), "Failed to create the image view");
}

void ImageView::destroy(const LogicalDevice& device) {
    if (isValid()) {
        vkDestroyImageView(device.get(), rawHandle(), nullptr);
    }
    resetHandle();
}
