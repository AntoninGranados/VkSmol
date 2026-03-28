/*
PER FRAME BUFFER
A storage for per frame buffers (one buffer per frame)
*/

#pragma once

#include <vulkan/vulkan.h>

#include <array>

#include "buffer.hpp"

#include "../utils.hpp"
#include "../frame_context.hpp"

class PhysicalDevice;
class LogicalDevice;
struct FrameContext;

class PerFrameBufferBase {
public:
    Buffer& at(const uint32_t& frameIndex) { return buffers[frameIndex]; }
    const Buffer& at(const uint32_t& frameIndex) const { return buffers[frameIndex]; }

protected:
    std::array<Buffer, MAX_FRAME_IN_FLIGHT> buffers;
};

template <typename T>
class PerFrameBuffer: public PerFrameBufferBase {
public:
    static PerFrameBuffer<T> create(const LogicalDevice& device, const PhysicalDevice& physicalDevice, const VkBufferUsageFlags& usage, size_t count) {
        PerFrameBuffer<T> buffer;
        for (size_t i = 0; i < MAX_FRAME_IN_FLIGHT; i++) {
            buffer.buffers[i].init(device, usage, sizeof(T) * count, true);
            buffer.buffers[i].alloc(device, physicalDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);    //? Not sure if these are the correct properties
        }
        return buffer;
    }

    void destroy(const LogicalDevice& device) {
        for (size_t i = 0; i < MAX_FRAME_IN_FLIGHT; i++) {
            buffers[i].free(device);
            buffers[i].destroy(device);
        }
    }

    Buffer& current(const FrameContext& frame) { return buffers[frame.currentFrame]; }
    const Buffer& current(const FrameContext& frame) const { return buffers[frame.currentFrame]; }
};
