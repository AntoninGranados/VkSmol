#pragma once

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace VkSmol {

constexpr uint32_t AllMips = VK_REMAINING_MIP_LEVELS;
constexpr uint32_t AllLayers = VK_REMAINING_ARRAY_LAYERS;

struct ImageSubresourceRange {
    uint32_t baseMip = 0;
    uint32_t mipCount = AllMips;
    uint32_t baseLayer = 0;
    uint32_t layerCount = AllLayers;

    bool operator==(const ImageSubresourceRange&) const = default;

    bool overlaps(const ImageSubresourceRange& other) const;
    bool contains(const ImageSubresourceRange& other) const;
    VkImageSubresourceRange toVk(VkImageAspectFlags aspect) const;
    std::string toString() const;
};

struct ImageSubresourceLayout {
    uint32_t mipCount = 1;
    uint32_t layerCount = 1;

    uint32_t count() const { return mipCount * layerCount; }
    uint32_t index(uint32_t mip, uint32_t layer) const { return mip * layerCount + layer; }
    std::optional<ImageSubresourceRange> resolve(const ImageSubresourceRange& range) const;
};

uint32_t maxMipCount(VkExtent2D extent);
VkExtent2D mipExtent(VkExtent2D extent, uint32_t mip);
size_t texelCount(VkExtent2D extent, const ImageSubresourceRange& range);

std::vector<VkBufferImageCopy> bufferImageCopyRegions(
    VkExtent2D extent, const ImageSubresourceRange& range, VkImageAspectFlags aspect, size_t bufferOffset,
    size_t texelSize
);
std::vector<VkImageCopy> imageCopyRegions(
    VkExtent2D sourceExtent, const ImageSubresourceRange& source, const ImageSubresourceRange& destination,
    VkImageAspectFlags aspect
);

} // namespace VkSmol
