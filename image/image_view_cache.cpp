#include "image_view_cache.hpp"

#include <format>
#include <stdexcept>

#include "../device/device.hpp"
#include "image.hpp"

namespace VkSmol {

VkImageView
ImageViewCache::getOrCreate(Device& device, Image& image, VkImageViewType type, const ImageSubresourceRange& range) {
    if (type != VK_IMAGE_VIEW_TYPE_2D && type != VK_IMAGE_VIEW_TYPE_2D_ARRAY)
        throw std::runtime_error(
            std::format("The image [{}] only supports 2D and 2D array views", image.getResourceName())
        );
    if (type == VK_IMAGE_VIEW_TYPE_2D && range.layerCount != 1)
        throw std::runtime_error(
            std::format(
                "A 2D view of the image [{}] needs a single layer, got [{}]", image.getResourceName(), range.toString()
            )
        );

    for (const Entry& entry : entries)
        if (entry.type == type && entry.range == range) return entry.view.get();

    Entry& entry = entries.emplace_back(
        Entry{
            .type = type,
            .range = range,
            .view = device.createImageView(image, type, range.toVk()),
        }
    );
    return entry.view.get();
}

void ImageViewCache::destroy(Device& device) {
    for (Entry& entry : entries) device.destroyImageView(entry.view);
    entries.clear();
}

} // namespace VkSmol
