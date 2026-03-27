#include "descriptor_writer.hpp"

#include <stdexcept>

// Public
DescriptorWriter& DescriptorWriter::uniform(uint32_t binding, const Buffer& buffer) {
    if (bindingFilled[binding]) {
        throw std::runtime_error("Binding is already filled");
    }
    if (findBinding(binding)->descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
        throw std::runtime_error("Expected a uniform buffer in the writer");
    }
    
    bindingFilled[binding] = true;
    writes.push_back({
        .binding = binding,
        .payload = UniformBufferPayload{
            .buffer = &buffer,
        }
    });
    
    return *this;
}

DescriptorWriter& DescriptorWriter::storage(uint32_t binding, const Buffer& buffer) {
    if (bindingFilled[binding]) {
        throw std::runtime_error("Binding is already filled");
    }
    if (findBinding(binding)->descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
        throw std::runtime_error("Expected a storage buffer in the writer");
    }
    
    bindingFilled[binding] = true;
    writes.push_back({
        .binding = binding,
        .payload = StorageBufferPayload{
            .buffer = &buffer,
        }
    });
    
    return *this;
}

DescriptorWriter& DescriptorWriter::sampler(uint32_t binding, const ImageView& view, const Sampler& sampler) {
    if (bindingFilled[binding]) {
        throw std::runtime_error("Binding is already filled");
    }
    if (findBinding(binding)->descriptorType != VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
        throw std::runtime_error("Expected a combined image sampler in the writer");
    }

    bindingFilled[binding] = true;
    writes.push_back({
        .binding = binding,
        .payload = CombinedImageSamplerPayload{
            .view = &view,
            .sampler = &sampler,
        }
    });

    return *this;
}

const std::vector<DescriptorWrite>& DescriptorWriter::getWrites() const {
    for (const bool& filled : bindingFilled) {
        if (!filled) {
            throw std::runtime_error("Not all bindings were filled");
        }
    }
    return writes;
}

// Private helpers
const VkDescriptorSetLayoutBinding* DescriptorWriter::findBinding(uint32_t binding) const {
    for (const auto& b : layout->getBindings()) {
        if (b.binding == binding) return &b;
    }
    throw std::runtime_error("Binding not found in descriptor layout");
}
