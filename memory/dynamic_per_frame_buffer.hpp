/*
DYNAMIC PER FRAME BUFFER
A storage for per frame dynamically resizable buffers
*/

// @note this storage don't have a destroy function as it shoud not handle buffer destruction, this is the role of the ResourceManager

#pragma once

#include <cmath>
#include <type_traits>
#include <variant>
#include <vulkan/vulkan.h>

#include "buffer.hpp"
#include "per_frame_buffer.hpp"
#include "../frame_context.hpp"

class ResourceManager;

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
    size_t getCapacityFromCount(size_t count) {
        if (count > 0) {
            size_t capacity = 1 << static_cast<size_t>(std::ceil(std::log2(count)));
            return capacity > 16 ? capacity : 16;
        }
        return 16;
    }
    size_t getCount() { return count; }

private:
    static DynamicPerFrameBuffer<T, Header> create(const VkBufferUsageFlags& usage, size_t count) {
        DynamicPerFrameBuffer<T, Header> buffer;
        buffer.usage = usage;
        buffer.count = count;
        buffer.capacity = buffer.getCapacityFromCount(buffer.count);

        buffer.byteSize = sizeof(T) * buffer.capacity;
        if (!std::is_same_v<Header, std::monostate>) buffer.byteSize += sizeof(Header);

        return buffer;
    }

    void clear() {
        count = 0;
        capacity = 16;

        byteSize = sizeof(T) * capacity;
        if (!std::is_same_v<Header, std::monostate>) byteSize += sizeof(Header);
    }

    bool setElementCount(size_t newCount) {
        count = newCount;
        size_t newCapacity = getCapacityFromCount(count);
        if (newCapacity > capacity) {
            capacity = newCapacity;

            byteSize = sizeof(T) * capacity;
            if (!std::is_same_v<Header, std::monostate>) byteSize += sizeof(Header);

            return true;
        }
        return false;
    }

    size_t capacity = 0;
    size_t count = 0;
    
    friend class ResourceManager;
};
