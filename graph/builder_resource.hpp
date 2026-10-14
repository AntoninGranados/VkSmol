#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <optional>
#include <utility>

#include "../render/shader.hpp"

namespace VkSmol {

// ================
// Handles
// ================

constexpr uint32_t InvalidHandle = uint32_t(-1);
struct PassHandle   { uint32_t id = InvalidHandle; };
struct ImageHandle  { uint32_t id = InvalidHandle; bool operator==(const ImageHandle&) const = default; };
struct BufferHandle { uint32_t id = InvalidHandle; bool operator==(const BufferHandle&) const = default; };

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

// @note not used for now
struct ImageSubresourceRangeInfo {
    uint32_t baseMipLevel = 0;
    uint32_t levelCount = 1;
    uint32_t baseArrayLayer = 0;
    uint32_t layerCount = 1;
};

struct ImageAccessInfo {
    ImageUsageType usage = ImageUsageType::Undefined;
    AccessType access = AccessType::None;
    WriteMode writeMode = WriteMode::Overwrite;
    AttachmentLoad attachmentLoad = AttachmentLoad::DontCare;
    ImageSubresourceRangeInfo subresourceRangeInfo{};
};

struct ImageAccess {
    ImageHandle image;
    ImageAccessInfo info;
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
    uint32_t           set;
    uint32_t           binding;
    bool               isImage;
    uint32_t           handleId;
    VkDescriptorType   descriptorType;
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
    uint32_t    passId = InvalidHandle;
};

struct GraphicsPipelineInfo {
    std::string name;
    VkVertexInputBindingDescription                            bindingDescription;
    std::vector<VkVertexInputAttributeDescription>             attributeDescriptions;
    std::vector<std::pair<VkShaderStageFlagBits, std::string>> shaderPaths;
    ShaderDefines shaderDefines;
    uint32_t    passId = InvalidHandle;
};

// ================
// Pass
// ================

struct NamedBinding {
    std::string name;
    bool        isImage;
    uint32_t    handleId;
    AccessType  access;
};

struct PassInfo {
    std::string name;
    PassType type;
    std::vector<ImageAccess>           imageAccesses      = {};
    std::vector<BufferAccess>          bufferAccesses     = {};
    std::vector<DescriptorBindingDecl> descriptorBindings = {};
    std::vector<NamedBinding>          namedBindings      = {};
    std::array<float, 4>               clearColor         = { 0.0f, 0.0f, 0.0f, 1.0f };
    VkIndexType                        indexType          = VK_INDEX_TYPE_UINT32;
    std::function<void(PassContext&)>  execute;
};

}
