#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
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
struct PassHandle            { uint32_t id = InvalidHandle; };
struct ImageHandle           { uint32_t id = InvalidHandle; };
struct BufferHandle          { uint32_t id = InvalidHandle; };
struct SubmissionGroupHandle { uint32_t id = InvalidHandle; };
struct TimestampHandle       { uint32_t id = InvalidHandle; };

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

enum VkSmolImageOwnership {
    VKSMOL_IMAGE_OWNERSHIP_TRANSIENT,  // Executor creates it, starts UNDEFINED, first access must be a write
    VKSMOL_IMAGE_OWNERSHIP_MANAGED,    // Executor creates it, starts in the declared initialAccess layout, can be read first
    VKSMOL_IMAGE_OWNERSHIP_IMPORTED,   // Caller provides the VkImage/VkImageView each frame via bindImage
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

struct ImageInfo {
    std::string name;

    VkFormat format = VK_FORMAT_UNDEFINED;
    uint32_t width = 1;
    uint32_t height = 1;
    uint32_t depth = 1;

    VkSmolImageOwnership ownership = VKSMOL_IMAGE_OWNERSHIP_TRANSIENT;
    std::optional<ImageAccessInfo> initialAccess = {};
    std::optional<ImageAccessInfo> finalAccess = {};

    // Additional VkImageUsageFlags ORed on top of the flags inferred from
    // declared pass accesses (e.g. for ping-pong images that need usage flags
    // their handle never directly declares).
    VkImageUsageFlags extraUsageFlags = 0;
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

enum VkSmolBufferOwnership {
    VKSMOL_BUFFER_OWNERSHIP_MANAGED,
    VKSMOL_BUFFER_OWNERSHIP_IMPORTED,  // Caller provides the VkBuffer each frame via bindBuffer
};

struct BufferAccessInfo {
    BufferUsageType usage = BufferUsageType::Storage;
    AccessType access = AccessType::Read;
};

struct BufferAccess {
    BufferHandle buffer;
    BufferAccessInfo info;
};

struct BufferInfo {
    std::string name;
    size_t size = 0;
    VkSmolBufferOwnership ownership = VKSMOL_BUFFER_OWNERSHIP_MANAGED;
    // Additional VkBufferUsageFlags ORed on top of the flags inferred from declared pass accesses
    VkBufferUsageFlags extraUsageFlags = 0;
    // Non-empty → device-local buffer uploaded once via staging at init; not CPU-writable after that.
    std::vector<uint8_t> initialData;
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

struct SubmissionGroupInfo {
    std::string name;
};

struct PassInfo {
    std::string name;
    PassType type;
    SubmissionGroupHandle group = {};
    std::vector<ImageAccess>           imageAccesses      = {};
    std::vector<BufferAccess>          bufferAccesses     = {};
    std::vector<DescriptorBindingDecl> descriptorBindings = {};
    std::optional<uint32_t>            timestampIndex;
};

}
