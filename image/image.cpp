#include "image.hpp"

#include "../device/logical_device.hpp"
#include "../device/physical_device.hpp"
#include "../memory/buffer.hpp"
#include "../utils.hpp"

namespace VkSmol {

Image::Image(Image&& other)
    : image(other.image), imageMemory(other.imageMemory), format(other.format), width(other.width),
      height(other.height), mipLevels(other.mipLevels), arrayLayers(other.arrayLayers), owned(other.owned) {
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
    const LogicalDevice& device, PhysicalDevice physicalDevice, uint32_t width_, uint32_t height_, VkFormat format_,
    VkImageUsageFlags usage, VkMemoryPropertyFlags properties, uint32_t mipLevels_, uint32_t arrayLayers_
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

void Image::destroy(const LogicalDevice& device) {
    if (imageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device.get(), imageMemory, nullptr);
    }
    imageMemory = VK_NULL_HANDLE;

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

void Sampler::init(const LogicalDevice& device, PhysicalDevice physicalDevice, bool anisotropy) {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physicalDevice.get(), &properties);
    samplerInfo.anisotropyEnable = anisotropy ? VK_TRUE : VK_FALSE;
    samplerInfo.maxAnisotropy = anisotropy ? properties.limits.maxSamplerAnisotropy : 1.0f;

    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    vkCheck(vkCreateSampler(device.get(), &samplerInfo, nullptr, &rawHandle()), "Failed to create the sampler");
}

void Sampler::destroy(const LogicalDevice& device) {
    if (isValid()) {
        vkDestroySampler(device.get(), rawHandle(), nullptr);
    }
    resetHandle();
}

}
