#include "image_view_cache.hpp"

#include "../device/device.hpp"
#include "image.hpp"
#include "image_format.hpp"

namespace VkSmol {

VkImageView
ImageViewCache::getOrCreate(Device& device, Image& image, VkImageViewType type, const ImageSubresourceRange& range) {
    for (const Entry& entry : entries)
        if (entry.type == type && entry.range == range) return entry.view.get();

    Entry& entry = entries.emplace_back(
        Entry{
            .type = type,
            .range = range,
            .view = device.createImageView(image, type, range.toVk(formatViewAspect(image.getFormat()))),
        }
    );
    return entry.view.get();
}

void ImageViewCache::destroy(Device& device) {
    for (Entry& entry : entries) device.destroyImageView(entry.view);
    entries.clear();
}

} // namespace VkSmol
