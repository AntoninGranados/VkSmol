#include "image_subresource.hpp"

#include <algorithm>
#include <bit>
#include <format>

namespace VkSmol {

bool ImageSubresourceRange::overlaps(const ImageSubresourceRange& other) const {
    const bool mips = baseMip < other.baseMip + other.mipCount && other.baseMip < baseMip + mipCount;
    const bool layers = baseLayer < other.baseLayer + other.layerCount && other.baseLayer < baseLayer + layerCount;
    return mips && layers;
}

VkImageSubresourceRange ImageSubresourceRange::toVk() const {
    return {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = baseMip,
        .levelCount = mipCount,
        .baseArrayLayer = baseLayer,
        .layerCount = layerCount,
    };
}

std::string ImageSubresourceRange::toString() const {
    const auto span = [](const char* name, uint32_t base, uint32_t count) {
        if (count == VK_REMAINING_MIP_LEVELS) return std::format("{}s {}..", name, base);
        if (count == 1) return std::format("{} {}", name, base);
        return std::format("{}s {}..{}", name, base, base + count - 1);
    };
    return span("mip", baseMip, mipCount) + ", " + span("layer", baseLayer, layerCount);
}

std::optional<ImageSubresourceRange> ImageSubresourceLayout::resolve(const ImageSubresourceRange& range) const {
    if (range.baseMip >= mipCount || range.baseLayer >= layerCount) return std::nullopt;

    ImageSubresourceRange resolved = range;
    if (resolved.mipCount == AllMips) resolved.mipCount = mipCount - range.baseMip;
    if (resolved.layerCount == AllLayers) resolved.layerCount = layerCount - range.baseLayer;

    if (resolved.mipCount == 0 || resolved.mipCount > mipCount - range.baseMip) return std::nullopt;
    if (resolved.layerCount == 0 || resolved.layerCount > layerCount - range.baseLayer) return std::nullopt;
    return resolved;
}

uint32_t maxMipCount(VkExtent2D extent) {
    return static_cast<uint32_t>(std::bit_width(std::max(extent.width, extent.height)));
}

VkExtent2D mipExtent(VkExtent2D extent, uint32_t mip) {
    return {std::max(1u, extent.width >> mip), std::max(1u, extent.height >> mip)};
}

size_t texelCount(VkExtent2D extent, const ImageSubresourceRange& range) {
    size_t count = 0;
    for (uint32_t mip = range.baseMip; mip < range.baseMip + range.mipCount; ++mip) {
        const VkExtent2D size = mipExtent(extent, mip);
        count += static_cast<size_t>(size.width) * size.height * range.layerCount;
    }
    return count;
}

std::vector<VkBufferImageCopy>
bufferImageCopyRegions(VkExtent2D extent, const ImageSubresourceRange& range, size_t bufferOffset, size_t texelSize) {
    std::vector<VkBufferImageCopy> regions;
    for (uint32_t mip = range.baseMip; mip < range.baseMip + range.mipCount; ++mip) {
        const VkExtent2D size = mipExtent(extent, mip);
        regions.push_back({
            .bufferOffset = bufferOffset,
            .imageSubresource =
                {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                 .mipLevel = mip,
                 .baseArrayLayer = range.baseLayer,
                 .layerCount = range.layerCount},
            .imageExtent = {size.width, size.height, 1},
        });
        bufferOffset += static_cast<size_t>(size.width) * size.height * range.layerCount * texelSize;
    }
    return regions;
}

std::vector<VkImageCopy> imageCopyRegions(
    VkExtent2D sourceExtent, const ImageSubresourceRange& source, const ImageSubresourceRange& destination
) {
    std::vector<VkImageCopy> regions;
    for (uint32_t i = 0; i < source.mipCount; ++i) {
        const VkExtent2D size = mipExtent(sourceExtent, source.baseMip + i);
        regions.push_back({
            .srcSubresource =
                {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                 .mipLevel = source.baseMip + i,
                 .baseArrayLayer = source.baseLayer,
                 .layerCount = source.layerCount},
            .dstSubresource =
                {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                 .mipLevel = destination.baseMip + i,
                 .baseArrayLayer = destination.baseLayer,
                 .layerCount = destination.layerCount},
            .extent = {size.width, size.height, 1},
        });
    }
    return regions;
}

} // namespace VkSmol
