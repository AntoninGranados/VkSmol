#pragma once

#include <vulkan/vulkan.h>

#include "../unique_handle.hpp"

class LogicalDevice;
class Image;

class ImageView: public UniqueHandle<VkImageView, VK_NULL_HANDLE> {
public:
    void init(const LogicalDevice& device, const Image& image);
    void destroy(const LogicalDevice& device);
};
