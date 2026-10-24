#include "resource_state.hpp"

#include <algorithm>
#include <utility>

#include "../graph/graph_utils.hpp"

namespace VkSmol {

namespace {

template <class AccessInfo> bool needsBarrier(const AccessInfo& from, const AccessInfo& to) {
    return from.usage != to.usage || from.access == AccessType::Write || to.access == AccessType::Write;
}

} // namespace

VkImageMemoryBarrier2 ImageTransition::toVk(VkImage image, VkImageAspectFlags aspect) const {
    const VulkanImageState src = imageAccessToVk(from);
    const VulkanImageState dst = imageAccessToVk(to);
    return {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = src.stage,
        .srcAccessMask = src.access,
        .dstStageMask = dst.stage,
        .dstAccessMask = dst.access,
        .oldLayout = src.layout,
        .newLayout = dst.layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = subresources.toVk(aspect),
    };
}

VkBufferMemoryBarrier2 BufferTransition::toVk(VkBuffer buffer) const {
    const VulkanBufferState src = bufferAccessToVk(from);
    const VulkanBufferState dst = bufferAccessToVk(to);
    return {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .srcStageMask = src.stage,
        .srcAccessMask = src.access,
        .dstStageMask = dst.stage,
        .dstAccessMask = dst.access,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = buffer,
        .offset = 0,
        .size = VK_WHOLE_SIZE,
    };
}

void ImageState::reset(const ImageSubresourceLayout& layout, const ImageAccessInfo& access) {
    accesses.assign(layout.count(), access);
}

std::vector<ImageTransition> ImageState::transition(
    const ImageSubresourceLayout& layout, const ImageSubresourceRange& range, const ImageAccessInfo& access
) {
    std::vector<ImageTransition> transitions;
    const uint32_t endLayer = range.baseLayer + range.layerCount;
    for (uint32_t mip = range.baseMip; mip < range.baseMip + range.mipCount; ++mip) {
        for (uint32_t layer = range.baseLayer; layer < endLayer;) {
            const ImageAccessInfo from = accesses[layout.index(mip, layer)];
            uint32_t next = layer + 1;
            while (next < endLayer && accesses[layout.index(mip, next)] == from) ++next;

            if (needsBarrier(from, access))
                transitions.push_back({
                    .subresources = {.baseMip = mip, .mipCount = 1, .baseLayer = layer, .layerCount = next - layer},
                    .from = from,
                    .to = access,
                });
            std::fill(accesses.begin() + layout.index(mip, layer), accesses.begin() + layout.index(mip, next), access);
            layer = next;
        }
    }
    return transitions;
}

void BufferState::reset() { current.reset(); }

std::optional<BufferTransition> BufferState::transition(const BufferAccessInfo& access) {
    const std::optional<BufferAccessInfo> from = std::exchange(current, access);
    if (!from || !needsBarrier(*from, access)) return std::nullopt;
    return BufferTransition{.from = *from, .to = access};
}

} // namespace VkSmol
