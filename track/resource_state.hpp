#pragma once

#include <vulkan/vulkan.h>

#include <optional>
#include <vector>

#include "../graph/builder_resource.hpp"
#include "../image/image_subresource.hpp"

namespace VkSmol {

struct ImageTransition {
    ImageSubresourceRange subresources;
    ImageAccessInfo from;
    ImageAccessInfo to;

    VkImageMemoryBarrier2 toVk(VkImage image, VkImageAspectFlags aspect) const;
};

struct BufferTransition {
    BufferAccessInfo from;
    BufferAccessInfo to;

    VkBufferMemoryBarrier2 toVk(VkBuffer buffer) const;
};

class ImageState {
public:
    void reset(const ImageSubresourceLayout& layout, const ImageAccessInfo& access = {});
    std::vector<ImageTransition>
    transition(const ImageSubresourceLayout& layout, const ImageSubresourceRange& range, const ImageAccessInfo& access);

private:
    std::vector<ImageAccessInfo> accesses;
};

class BufferState {
public:
    void reset();
    std::optional<BufferTransition> transition(const BufferAccessInfo& access);

private:
    std::optional<BufferAccessInfo> current;
};

} // namespace VkSmol
