/*
PER FRAME BUFFER
A storage for per frame buffers (one buffer per frame)
*/

// @note this storage don't have a destroy function as it shoud not handle buffer destruction, this is the role of the ResourceManager

#pragma once

#include <vulkan/vulkan.h>

#include <array>

#include "buffer.hpp"

#include "../utils.hpp"
#include "../frame_context.hpp"


class PhysicalDevice;
class LogicalDevice;
class ResourceManager;
struct FrameContext;

class PerFrameBufferBase {
public:
    Buffer& at(const uint32_t& frameIndex) { return buffers[frameIndex]; }
    const Buffer& at(const uint32_t& frameIndex) const { return buffers[frameIndex]; }

    size_t getByteSize() { return byteSize; }
    VkBufferUsageFlags getUsage() { return usage; }

protected:
    std::array<Buffer, MAX_FRAME_IN_FLIGHT> buffers;
    
    VkBufferUsageFlags usage;
    size_t byteSize;

    friend ResourceManager;
};

template <typename T>
class PerFrameBuffer: public PerFrameBufferBase {
public:
    static PerFrameBuffer<T> create(const VkBufferUsageFlags& usage, size_t count) {
        PerFrameBuffer<T> buffer;
        buffer.usage = usage;
        buffer.byteSize = sizeof(T) * count;
        return buffer;
    }

    Buffer& current(const FrameContext& frame) { return buffers[frame.currentFrame]; }
    const Buffer& current(const FrameContext& frame) const { return buffers[frame.currentFrame]; }
};
