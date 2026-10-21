#include "image.hpp"

#include "../device/logical_device.hpp"
#include "../device/physical_device.hpp"
#include "../memory/buffer.hpp"
#include "../utils.hpp"

namespace VkSmol {

Image::Image(Image&& other)
    : TrackedResource(std::move(other)), image(other.image), imageMemory(other.imageMemory), format(other.format),
      width(other.width), height(other.height), mipLevels(other.mipLevels), arrayLayers(other.arrayLayers),
      owned(other.owned) {
    other.image = VK_NULL_HANDLE;
    other.imageMemory = VK_NULL_HANDLE;
    other.format = VK_FORMAT_UNDEFINED;
    other.width = 0;
    other.height = 0;
    other.mipLevels = 1;
    other.arrayLayers = 1;
    other.owned = true;
}

Image& Image::operator=(Image&& other) {
    if (this == &other) return *this;

    if (image != VK_NULL_HANDLE || imageMemory != VK_NULL_HANDLE) {
        throw std::runtime_error("Image move assignment requires an empty destination");
    }

    TrackedResource::operator=(std::move(other));

    image = other.image;
    imageMemory = other.imageMemory;
    format = other.format;
    width = other.width;
    height = other.height;
    mipLevels = other.mipLevels;
    arrayLayers = other.arrayLayers;
    owned = other.owned;

    other.image = VK_NULL_HANDLE;
    other.imageMemory = VK_NULL_HANDLE;
    other.format = VK_FORMAT_UNDEFINED;
    other.width = 0;
    other.height = 0;
    other.mipLevels = 1;
    other.arrayLayers = 1;
    other.owned = true;

    return *this;
}

// Public
void Image::initExternal(VkImage handle, VkFormat fmt, uint32_t w, uint32_t h) {
    image  = handle;
    format = fmt;
    width  = w;
    height = h;
    owned  = false;
}

void Image::init(
    const LogicalDevice& device, uint32_t width_, uint32_t height_, VkFormat format_, VkImageUsageFlags usage,
    uint32_t mipLevels_, uint32_t arrayLayers_
) {
    width = width_;
    height = height_;
    format = format_;
    mipLevels = mipLevels_;
    arrayLayers = arrayLayers_;

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = arrayLayers;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    imageInfo.format = format;
    imageInfo.usage = usage;

    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCheck(vkCreateImage(device.get(), &imageInfo, nullptr, &image), "Failed to create image");
}

void Image::alloc(const LogicalDevice& device, PhysicalDevice physicalDevice, VkMemoryPropertyFlags properties) {
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device.get(), image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = Buffer::findMemoryType(
        physicalDevice,
        memRequirements.memoryTypeBits,
        properties
    );

    vkCheck(vkAllocateMemory(device.get(), &allocInfo, nullptr, &imageMemory), "Failed to allocate image memory");
    vkCheck(vkBindImageMemory(device.get(), image, imageMemory, 0), "Failed to bind image memory");
}

void Image::free(const LogicalDevice& device) {
    if (imageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device.get(), imageMemory, nullptr);
    }
    imageMemory = VK_NULL_HANDLE;
}

void Image::destroy(const LogicalDevice& device) {
    if (owned && image != VK_NULL_HANDLE) {
        vkDestroyImage(device.get(), image, nullptr);
    }
    image = VK_NULL_HANDLE;
    format = VK_FORMAT_UNDEFINED;
    width = 0;
    height = 0;
    mipLevels = 1;
    arrayLayers = 1;
    owned = true;
}

}
