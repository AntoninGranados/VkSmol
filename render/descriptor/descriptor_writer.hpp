#pragma once

#include <vulkan/vulkan.h>

#include <vector>
#include <unordered_set>
#include <variant>

#include "../../memory/buffer.hpp"
#include "../../image/image_view.hpp"
#include "../../image/sampler.hpp"

#include "./descriptor_set.hpp"
#include "./descriptor_set_layout.hpp"

namespace VkSmol {

struct UniformBufferPayload {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
    VkDeviceSize offset = 0;              //! Ignored for now
    VkDeviceSize range = VK_WHOLE_SIZE;   //! Ignored for now
};

struct StorageBufferPayload {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
    VkDeviceSize offset = 0;              //! Ignored for now
    VkDeviceSize range = VK_WHOLE_SIZE;   //! Ignored for now
};

struct CombinedImageSamplerPayload {
    VkImageView view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;  //! Ignored for now
};

struct StorageImagePayload {
    VkImageView view = VK_NULL_HANDLE;
};

using DescriptorPayload = std::variant<
    UniformBufferPayload,
    StorageBufferPayload,
    CombinedImageSamplerPayload,
    StorageImagePayload
>;

struct DescriptorWrite {
    uint32_t binding = 0;
    uint32_t arrayElement = 0;    //! Ignored for now
    DescriptorPayload payload;
};

class DescriptorWriter {
public:
    DescriptorWriter(const DescriptorSetLayout& layout): layout(&layout) {}

    DescriptorWriter& uniform(uint32_t binding, const Buffer& buffer);
    DescriptorWriter& uniform(uint32_t binding, VkBuffer buffer, VkDeviceSize size);
    DescriptorWriter& storage(uint32_t binding, const Buffer& buffer);
    DescriptorWriter& storage(uint32_t binding, VkBuffer buffer, VkDeviceSize size);
    DescriptorWriter& combinedImageSampler(uint32_t binding, const ImageView& view, const Sampler& sampler);
    DescriptorWriter& combinedImageSampler(uint32_t binding, VkImageView view, VkSampler sampler);
    DescriptorWriter& storageImage(uint32_t binding, const ImageView& view);
    DescriptorWriter& storageImage(uint32_t binding, VkImageView view);

    void validate() const;
    const std::vector<DescriptorWrite>& getWrites() const { return writes; }

private:
    const VkDescriptorSetLayoutBinding* findBinding(uint32_t binding) const;

    std::unordered_set<uint32_t> filledBinding;
    std::vector<DescriptorWrite> writes;
    const DescriptorSetLayout* layout = nullptr;
};

}
