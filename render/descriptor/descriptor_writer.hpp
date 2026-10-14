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
    const Buffer* buffer = nullptr;
    VkDeviceSize offset = 0;              //! Ignored for now
    VkDeviceSize range = VK_WHOLE_SIZE;   //! Ignored for now
};

struct StorageBufferPayload {
    const Buffer* buffer = nullptr;
    VkDeviceSize offset = 0;              //! Ignored for now
    VkDeviceSize range = VK_WHOLE_SIZE;   //! Ignored for now
};

struct CombinedImageSamplerPayload {
    const ImageView* view = nullptr;
    const Sampler* sampler = nullptr;
    VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;  //! Ignored for now
};

struct StorageImagePayload {
    const ImageView* view = nullptr;
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
    DescriptorWriter& storage(uint32_t binding, const Buffer& buffer);
    DescriptorWriter& combinedImageSampler(uint32_t binding, const ImageView& view, const Sampler& sampler);
    DescriptorWriter& storageImage(uint32_t binding, const ImageView& view);

    void validate() const;
    const std::vector<DescriptorWrite>& getWrites() const { return writes; }

private:
    const VkDescriptorSetLayoutBinding* findBinding(uint32_t binding) const;

    std::unordered_set<uint32_t> filledBinding;
    std::vector<DescriptorWrite> writes;
    const DescriptorSetLayout* layout = nullptr;
};

}
