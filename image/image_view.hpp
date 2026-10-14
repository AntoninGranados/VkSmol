#pragma once

#include <vulkan/vulkan.h>

#include "../base/unique_handle.hpp"
#include "../base/tracked_resource.hpp"

namespace VkSmol {

class LogicalDevice;
class Image;

class ImageView: public UniqueHandle<VkImageView, VK_NULL_HANDLE>, public TrackedResource {
public:
    void init(const LogicalDevice& device, const Image& image);
    void
    init(const LogicalDevice& device, const Image& image, VkImageViewType type, const VkImageSubresourceRange& range);
    void destroy(const LogicalDevice& device);
};

}
