#include "image_transition.hpp"

#include <algorithm>

#include "../graph/compiled_resource.hpp"
#include "../graph/graph_utils.hpp"

namespace VkSmol {

namespace {

bool needsBarrier(const ImageAccessInfo& from, const ImageAccessInfo& to) {
    return from.usage != to.usage || from.access == AccessType::Write || to.access == AccessType::Write;
}

} // namespace

VkImageMemoryBarrier2 ImageTransition::toVk(VkImage image) const {
    const VulkanImageState src = imageAccessToVk(from);
    const VulkanImageState dst = imageAccessToVk(to);
    VkImageSubresourceRange range = subresources.toVk();
    range.aspectMask = dst.aspect;
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
        .subresourceRange = range,
    };
}

std::vector<ImageTransition> transitionSubresources(
    std::vector<ImageAccessInfo>& accesses, const ImageSubresourceLayout& layout, const ImageSubresourceRange& range,
    const ImageAccessInfo& access
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

} // namespace VkSmol
