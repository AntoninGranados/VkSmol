#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>
#include <vector>

struct PassInfo;
struct ImageInfo;
struct ImageUsageInfo;
struct BufferInfo;
struct BufferUsageInfo;

constexpr uint32_t InvalidHandle = uint32_t(-1);
struct PassHandle { uint32_t id = InvalidHandle; };
struct ImageHandle { uint32_t id = InvalidHandle; };
struct BufferHandle { uint32_t id = InvalidHandle; };

enum class PassType : uint16_t {
    Graphics,
    // Compute,
    Transfer,
    Present,
};

struct PassInfo {
    std::string name;
    PassType type;
    std::vector<ImageUsageInfo> imageUsages = {};
    // std::vector<BufferUsageInfo> bufferUsages = {};
};

enum class AccessType : uint16_t {
    None,
    Read,
    Write,
    // ReadWrite,   // TODO implement support
};

// ================
// Image
// ================

enum class ImageUsage : uint16_t {
    Sampled,
    ColorAttachment,
    DepthAttachment,
    Present,
    TransferSrc,
    TransferDst,
};

struct ImageInfo {
    std::string name;
    
    VkFormat format = VK_FORMAT_UNDEFINED;
    uint32_t width = 1;
    uint32_t height = 1;
    uint32_t depth = 1;
    uint32_t arrayLayers = 1;
    uint32_t mipLevels = 1;
};

struct ImageUsageInfo {
    ImageHandle handle;
    ImageUsage usage;
    AccessType access = AccessType::None;
};

// ================
// Buffer
// ================

// TODO implement `BufferUsage`
enum class BufferUsage : uint16_t {
};

// TODO implement `BufferInfo`
struct BufferInfo {
    std::string name;
};

// TODO implement `BufferUsageInfo`
struct BufferUsageInfo {
    BufferHandle handle;
    BufferUsage usage;
    AccessType access;
};
