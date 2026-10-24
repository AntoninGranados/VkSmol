#pragma once

#include <vulkan/vulkan.h>

#include <vector>

#include "image_subresource.hpp"
#include "image.hpp"

namespace VkSmol {

class Device;
class Image;

class ImageViewCache {
public:
    VkImageView getOrCreate(Device& device, Image& image, VkImageViewType type, const ImageSubresourceRange& range);
    void destroy(Device& device);

private:
    struct Entry {
        VkImageViewType type;
        ImageSubresourceRange range;
        ImageView view;
    };

    std::vector<Entry> entries;
};

} // namespace VkSmol
