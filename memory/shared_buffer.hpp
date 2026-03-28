/*
SHARED BUFFER
A buffer that is shared across frames
*/

#pragma once

#include <variant>
#include <vulkan/vulkan.h>

#include "buffer.hpp"

class PhysicalDevice;
class LogicalDevice;

template <typename T, typename Header = std::monostate>
class SharedBuffer {
public:
    SharedBuffer() = default;
    SharedBuffer(const SharedBuffer&) = delete;
    SharedBuffer& operator=(const SharedBuffer&) = delete;
    SharedBuffer(SharedBuffer&&) = default;
    SharedBuffer& operator=(SharedBuffer&&) = default;

    Buffer& get() { return buffer; }
    const Buffer& get() const { return buffer; }

private:
    static SharedBuffer<T, Header> create(const LogicalDevice& device, const PhysicalDevice& physicalDevice, const VkBufferUsageFlags& usage, size_t count) {
        SharedBuffer<T, Header> buffer;

        size_t size = sizeof(T) * count;
        if (!std::is_same_v<Header, std::monostate>) size += sizeof(Header);

        buffer.buffer.init(device, usage, size, true);
        buffer.buffer.alloc(device, physicalDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        return buffer;
    }

    void destroy(const LogicalDevice& device) {
        buffer.free(device);
        buffer.destroy(device);
    }

    Buffer buffer;

    friend class VkSmol;
};
