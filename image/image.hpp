#pragma once

#include <vulkan/vulkan.h>

#include "../base/unique_handle.hpp"

namespace VkSmol {

class LogicalDevice;
class PhysicalDevice;

class Image {
public:
    Image() = default;
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;
    Image(Image&& other);
    Image& operator=(Image&& other);

    void init(
        const LogicalDevice& device, PhysicalDevice physicalDevice, uint32_t width, uint32_t height, VkFormat format,
        VkImageUsageFlags usage, VkMemoryPropertyFlags properties, uint32_t mipLevels = 1, uint32_t arrayLayers = 1
    );
    void initExternal(VkImage handle, VkFormat fmt, uint32_t w, uint32_t h);
    void destroy(const LogicalDevice& device);

    VkImage& get() { return image; }
    const VkImage& get() const { return image; }
    VkFormat& getFormat() { return format; }
    const VkFormat& getFormat() const { return format; }
    int getWidth() { return width; }
    int getHeight() { return height; }
    uint32_t getMipLevels() const { return mipLevels; }
    uint32_t getArrayLayers() const { return arrayLayers; }

private:
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory imageMemory = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    uint32_t width = 0, height = 0;
    uint32_t mipLevels = 1, arrayLayers = 1;
    bool owned = true;
};

class ImageView: public UniqueHandle<VkImageView, VK_NULL_HANDLE> {
public:
    void init(const LogicalDevice& device, const Image& image);
    void
    init(const LogicalDevice& device, const Image& image, VkImageViewType type, const VkImageSubresourceRange& range);
    void destroy(const LogicalDevice& device);
};

class Sampler: public UniqueHandle<VkSampler, VK_NULL_HANDLE> {
public:
    void init(const LogicalDevice& device, PhysicalDevice physicalDevice, bool anisotropy);
    void destroy(const LogicalDevice& device);
};

}
