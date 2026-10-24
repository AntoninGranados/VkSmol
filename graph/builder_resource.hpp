#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <optional>
#include <utility>

#include "../image/image_subresource.hpp"
#include "../render/pipeline/graphics_state.hpp"
#include "../render/shader.hpp"

namespace VkSmol {

// ================
// Handles
// ================

constexpr uint32_t InvalidHandle = uint32_t(-1);

struct ImageRange;

struct PassHandle   { uint32_t id = InvalidHandle; };
struct BufferHandle { uint32_t id = InvalidHandle; bool operator==(const BufferHandle&) const = default; };
struct ImageHandle {
    uint32_t id = InvalidHandle;
    bool operator==(const ImageHandle&) const = default;

    ImageRange mip(uint32_t index) const;
    ImageRange mips(uint32_t base, uint32_t count) const;
    ImageRange layer(uint32_t index) const;
    ImageRange layers(uint32_t base, uint32_t count) const;
};

struct ImageRange {
    ImageHandle image;
    ImageSubresourceRange subresources;

    ImageRange(ImageHandle image, ImageSubresourceRange subresources = {}) : image(image), subresources(subresources) {}

    ImageRange mip(uint32_t index) const { return mips(index, 1); }
    ImageRange mips(uint32_t base, uint32_t count) const {
        ImageRange range = *this;
        range.subresources.baseMip = base;
        range.subresources.mipCount = count;
        return range;
    }
    ImageRange layer(uint32_t index) const { return layers(index, 1); }
    ImageRange layers(uint32_t base, uint32_t count) const {
        ImageRange range = *this;
        range.subresources.baseLayer = base;
        range.subresources.layerCount = count;
        return range;
    }
};

inline ImageRange ImageHandle::mip(uint32_t index) const { return ImageRange(*this).mip(index); }
inline ImageRange ImageHandle::mips(uint32_t base, uint32_t count) const { return ImageRange(*this).mips(base, count); }
inline ImageRange ImageHandle::layer(uint32_t index) const { return ImageRange(*this).layer(index); }
inline ImageRange ImageHandle::layers(uint32_t base, uint32_t count) const {
    return ImageRange(*this).layers(base, count);
}

class PassContext;

enum class PassType : uint16_t {
    Graphics,
    Compute,
    Transfer,
    Present,
};

enum class AccessType : uint16_t {
    None,
    Read,
    Write,
};

// ================
// Image
// ================

enum class ImageUsageType : uint16_t {
    Undefined,
    Sampled,
    Storage,
    ColorAttachment,
    DepthAttachment,
    Present,
    TransferSrc,
    TransferDst,
};

enum class WriteMode : uint16_t {
    Overwrite,
    Preserve,
};

enum class AttachmentLoad : uint16_t {
    DontCare,
    Clear,
    Load,
};

struct ImageAccessInfo {
    ImageUsageType usage = ImageUsageType::Undefined;
    AccessType access = AccessType::None;
    WriteMode writeMode = WriteMode::Overwrite;
    AttachmentLoad attachmentLoad = AttachmentLoad::DontCare;
    bool operator==(const ImageAccessInfo&) const = default;
};

struct ImageAccess {
    ImageHandle image;
    ImageSubresourceRange subresources;
    ImageAccessInfo info;
    VkClearValue clearValue = {};
};

// ================
// Buffer
// ================

enum class BufferUsageType : uint16_t {
    Storage,
    Uniform,
    TransferSrc,
    TransferDst,
    Vertex,
    Index,
    Indirect,
};

struct BufferAccessInfo {
    BufferUsageType usage = BufferUsageType::Storage;
    AccessType access = AccessType::Read;
};

struct BufferAccess {
    BufferHandle buffer;
    BufferAccessInfo info;
};

// ================
// Descriptor bindings
// ================

struct DescriptorBindingDecl {
    uint32_t set;
    uint32_t binding;
    bool isImage;
    uint32_t handleId;
    ImageSubresourceRange subresources;
    VkImageViewType viewType;
    VkDescriptorType descriptorType;
    VkShaderStageFlags stageFlags;
};

// ================
// Pipeline
// ================

struct ComputePipelineHandle  { uint32_t id = InvalidHandle; };
struct GraphicsPipelineHandle { uint32_t id = InvalidHandle; };

struct ComputePipelineInfo {
    std::string name;
    std::string shaderPath;
    ShaderDefines shaderDefines;
    ShaderConformances shaderConformances;
    uint32_t    passId = InvalidHandle;
};

struct GraphicsPipelineInfo {
    std::string name;
    VkVertexInputBindingDescription                            bindingDescription;
    std::vector<VkVertexInputAttributeDescription>             attributeDescriptions;
    std::vector<std::pair<VkShaderStageFlagBits, std::string>> shaderPaths;
    ShaderDefines shaderDefines;
    ShaderConformances shaderConformances;
    GraphicsState state;
    uint32_t    passId = InvalidHandle;
};

// ================
// Pass
// ================

struct NamedBinding {
    std::string name;
    bool isImage;
    uint32_t handleId;
    ImageSubresourceRange subresources;
    AccessType access;
};

struct PassInfo {
    uint32_t id;
    std::string name;
    PassType type;
    std::vector<ImageAccess>           imageAccesses      = {};
    std::vector<BufferAccess>          bufferAccesses     = {};
    std::vector<DescriptorBindingDecl> descriptorBindings = {};
    std::vector<NamedBinding>          namedBindings      = {};
    VkIndexType                        indexType          = VK_INDEX_TYPE_UINT32;
    std::optional<uint32_t> pipelineId = {};
    std::function<void(PassContext&)>  execute;
};

struct GraphDeclaration {
    std::vector<PassInfo> passes;
    std::vector<ComputePipelineInfo> computePipelines;
    std::vector<GraphicsPipelineInfo> graphicsPipelines;
};

}
