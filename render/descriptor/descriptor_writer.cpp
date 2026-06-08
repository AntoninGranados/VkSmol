#include "descriptor_writer.hpp"

#include <sstream>
#include <stdexcept>

namespace {

const char* descriptorTypeName(VkDescriptorType type) {
    switch (type) {
        case VK_DESCRIPTOR_TYPE_SAMPLER:
            return "VK_DESCRIPTOR_TYPE_SAMPLER";
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            return "VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER";
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            return "VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE";
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            return "VK_DESCRIPTOR_TYPE_STORAGE_IMAGE";
        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            return "VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER";
        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
            return "VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER";
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            return "VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER";
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            return "VK_DESCRIPTOR_TYPE_STORAGE_BUFFER";
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
            return "VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC";
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
            return "VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC";
        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            return "VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT";
        default:
            return "VK_DESCRIPTOR_TYPE_UNKNOWN";
    }
}

std::runtime_error duplicateBindingError(uint32_t binding) {
    std::ostringstream message;
    message << "DescriptorWriter: binding " << binding << " is already assigned";
    return std::runtime_error(message.str());
}

std::runtime_error descriptorTypeMismatchError(uint32_t binding, VkDescriptorType expected, VkDescriptorType actual) {
    std::ostringstream message;
    message << "DescriptorWriter: binding " << binding
            << " is declared as " << descriptorTypeName(actual)
            << ", but this write requires " << descriptorTypeName(expected);
    return std::runtime_error(message.str());
}

} // namespace

// Public
DescriptorWriter& DescriptorWriter::uniform(uint32_t binding, const Buffer& buffer) {
    if (filledBinding.contains(binding)) {
        throw duplicateBindingError(binding);
    }
    const VkDescriptorSetLayoutBinding* layoutBinding = findBinding(binding);
    if (layoutBinding->descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
        throw descriptorTypeMismatchError(binding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, layoutBinding->descriptorType);
    }
    
    filledBinding.emplace(binding);
    writes.push_back({
        .binding = binding,
        .payload = UniformBufferPayload{
            .buffer = &buffer,
        }
    });
    
    return *this;
}

DescriptorWriter& DescriptorWriter::storage(uint32_t binding, const Buffer& buffer) {
    if (filledBinding.contains(binding)) {
        throw duplicateBindingError(binding);
    }
    const VkDescriptorSetLayoutBinding* layoutBinding = findBinding(binding);
    if (layoutBinding->descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
        throw descriptorTypeMismatchError(binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, layoutBinding->descriptorType);
    }
    
    filledBinding.emplace(binding);
    writes.push_back({
        .binding = binding,
        .payload = StorageBufferPayload{
            .buffer = &buffer,
        }
    });
    
    return *this;
}

DescriptorWriter& DescriptorWriter::combinedImageSampler(uint32_t binding, const ImageView& view, const Sampler& sampler) {
    if (filledBinding.contains(binding)) {
        throw duplicateBindingError(binding);
    }
    const VkDescriptorSetLayoutBinding* layoutBinding = findBinding(binding);
    if (layoutBinding->descriptorType != VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
        throw descriptorTypeMismatchError(binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, layoutBinding->descriptorType);
    }

    filledBinding.emplace(binding);
    writes.push_back({
        .binding = binding,
        .payload = CombinedImageSamplerPayload{
            .view = &view,
            .sampler = &sampler,
        }
    });

    return *this;
}

DescriptorWriter& DescriptorWriter::storageImage(uint32_t binding, const ImageView& view) {
    if (filledBinding.contains(binding)) {
        throw duplicateBindingError(binding);
    }
    const VkDescriptorSetLayoutBinding* layoutBinding = findBinding(binding);
    if (layoutBinding->descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
        throw descriptorTypeMismatchError(binding, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, layoutBinding->descriptorType);
    }

    filledBinding.emplace(binding);
    writes.push_back({
        .binding = binding,
        .payload = StorageImagePayload{
            .view = &view,
        }
    });

    return *this;
}

void DescriptorWriter::validate() const {
    for (const auto& b : layout->getBindings()) {
        if (!filledBinding.contains(b.binding)) {
            std::ostringstream message;
            message << "DescriptorWriter: missing binding " << b.binding
                    << " (" << descriptorTypeName(b.descriptorType) << ")";
            throw std::runtime_error(message.str());
        }
    }
}

// Private helpers
const VkDescriptorSetLayoutBinding* DescriptorWriter::findBinding(uint32_t binding) const {
    std::ostringstream availableBindings;
    bool first = true;
    for (const auto& b : layout->getBindings()) {
        if (b.binding == binding) return &b;
        if (!first) availableBindings << ", ";
        availableBindings << b.binding;
        first = false;
    }
    std::ostringstream message;
    message << "DescriptorWriter: binding " << binding << " does not exist in the descriptor layout";
    if (!first) {
        message << " (available bindings: " << availableBindings.str() << ")";
    }
    throw std::runtime_error(message.str());
}
