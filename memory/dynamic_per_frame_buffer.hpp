/*
DYNAMIC PER FRAME BUFFER
A storage for per frame dynamically resizable buffers
*/

#pragma once

#include <cmath>
#include <type_traits>
#include <variant>
#include <vulkan/vulkan.h>

#include "buffer.hpp"
#include "per_frame_buffer.hpp"
#include "../utils.hpp"
#include "../frame_context.hpp"

class PhysicalDevice;
class LogicalDevice;
class VkSmol;

template <typename T, typename Header = std::monostate>
class DynamicPerFrameBuffer: public PerFrameBufferBase {
public:
    DynamicPerFrameBuffer() = default;
    DynamicPerFrameBuffer(const DynamicPerFrameBuffer&) = delete;
    DynamicPerFrameBuffer& operator=(const DynamicPerFrameBuffer&) = delete;
    DynamicPerFrameBuffer(DynamicPerFrameBuffer&&) = default;
    DynamicPerFrameBuffer& operator=(DynamicPerFrameBuffer&&) = default;

    Buffer& current(const FrameContext& frame) { return buffers[frame.currentFrame]; }
    const Buffer& current(const FrameContext& frame) const { return buffers[frame.currentFrame]; }

    size_t getCapacity() { return capacity; }
    size_t getCount() { return count; }

private:
    static DynamicPerFrameBuffer<T, Header> create(const LogicalDevice& device, const PhysicalDevice& physicalDevice, const VkBufferUsageFlags& usage, size_t count) {
        DynamicPerFrameBuffer<T, Header> buffer;
        buffer.usage = usage;
        buffer.count = count;
        buffer.capacity = 16;
        if (count > 0) {
            buffer.capacity = 1 << static_cast<size_t>(std::ceil(std::log2(count)));
            buffer.capacity = buffer.capacity > 16 ? buffer.capacity : 16;
        }
        
        for (size_t i = 0; i < MAX_FRAME_IN_FLIGHT; i++) {
            size_t size = sizeof(T) * buffer.capacity;
            if (!std::is_same_v<Header, std::monostate>) size += sizeof(Header);

            buffer.buffers[i].init(device, usage, size, true);
            buffer.buffers[i].alloc(device, physicalDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        }

        return buffer;
    }

    void destroy(const LogicalDevice& device) {
        for (size_t i = 0; i < MAX_FRAME_IN_FLIGHT; i++) {
            buffers[i].free(device);
            buffers[i].destroy(device);
        }
        count = 0;
        capacity = 0;
    }

    void clear(const LogicalDevice& device, const PhysicalDevice& physicalDevice) {
        destroy(device);
        count = 0;
        capacity = 16;
        for (size_t i = 0; i < MAX_FRAME_IN_FLIGHT; i++) {
            size_t size = sizeof(T) * capacity;
            if (!std::is_same_v<Header, std::monostate>) size += sizeof(Header);

            buffers[i].init(device, usage, size, true);
            buffers[i].alloc(device, physicalDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        }
    }

    bool setElementCount(const LogicalDevice& device, const PhysicalDevice& physicalDevice, size_t newCount) {
        count = newCount;

        // Find nearest power of two
        size_t newCapacity = 16;
        if (count > 0) {
            newCapacity = 1 << static_cast<size_t>(std::ceil(std::log2(count)));
            newCapacity = newCapacity > 16 ? newCapacity : 16;
        }

        if (newCapacity > capacity) {
            resize(device, physicalDevice, newCapacity);
            return true;
        }
        return false;
    }

    void resize(const LogicalDevice& device, const PhysicalDevice& physicalDevice, size_t newCapacity) {
        destroy(device);
        capacity = newCapacity;
        for (size_t i = 0; i < MAX_FRAME_IN_FLIGHT; i++) {
            size_t size = sizeof(T) * capacity;
            if (!std::is_same_v<Header, std::monostate>) size += sizeof(Header);

            buffers[i].init(device, usage, size, true);
            buffers[i].alloc(device, physicalDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        }
    }

    size_t capacity = 0;
    size_t count = 0;
    VkBufferUsageFlags usage = 0;
    
    friend class VkSmol;
};
