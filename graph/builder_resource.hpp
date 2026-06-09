#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>
#include <vector>
#include <optional>

struct PassInfo;
struct ImageInfo;
struct ImageAccess;

constexpr uint32_t InvalidHandle = uint32_t(-1);
struct PassHandle { uint32_t id = InvalidHandle; };
struct ImageHandle { uint32_t id = InvalidHandle; };
// struct BufferHandle { uint32_t id = InvalidHandle; };

enum class PassType : uint16_t {
    Graphics,
    Compute,
    Transfer,
    Present,
};

struct PassInfo {
    std::string name;
    PassType type;
    std::vector<ImageAccess> imageAccesses = {};
    // std::vector<BufferUsageInfo> bufferUsages = {};
};

enum class AccessType : uint16_t {
    None,
    Read,
    Write,
    // ReadWrite,   // @todo implement support
};

// ================
// Image
// ================

enum class ImageUsageType : uint16_t {
    Undefined,
    Sampled,
    StorageImage,
    ColorAttachment,
    DepthAttachment,
    Present,
    TransferSrc,
    TransferDst,
};

enum class ImageOwnership : uint16_t {
    Transient,  // Executor creates it; starts UNDEFINED; first access must be a write
    Managed,    // Executor creates it; starts in declared initialAccess layout; can be read first
    Imported,   // Caller provides via bindImage each frame; executor never creates GPU resources
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

    ImageOwnership ownership = ImageOwnership::Transient;
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

/*
// @todo implement `BufferUsage`
enum class BufferUsage : uint16_t {
};

// @todo implement `BufferInfo`
struct BufferInfo {
    std::string name;
};

// @todo implement `BufferUsageInfo`
struct BufferUsageInfo {
    BufferHandle handle;
    BufferUsage usage;
    AccessType access;
};
*/
