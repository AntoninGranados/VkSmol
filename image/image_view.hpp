#pragma once

#include <vulkan/vulkan.h>

#include "../base/unique_handle.hpp"
#include "../base/tracked_resource.hpp"
#include "engine/resource_registry.hpp"

class LogicalDevice;
class Image;

class ImageView: public UniqueHandle<VkImageView, VK_NULL_HANDLE>, public TrackedResource<ResourceType::ImageView> {
public:
    void init(const LogicalDevice& device, const Image& image);
    void destroy(const LogicalDevice& device);
};
