/*
SHARED BUFFER
A buffer that is shared across frames
*/

// @note this storage don't have a destroy function as it shoud not handle buffer destruction, this is the role of the ResourceManager

#pragma once

#include <variant>
#include <vulkan/vulkan.h>

#include "buffer.hpp"

class PhysicalDevice;
class LogicalDevice;
class ResourceManager;

class SharedBufferBase {
public:
    Buffer& get() { return buffer; }
    const Buffer& get() const { return buffer; }

    size_t getByteSize() { return byteSize; }
    VkBufferUsageFlags getUsage() { return usage; }

protected:
    Buffer buffer;

    VkBufferUsageFlags usage;
    size_t byteSize;

    friend class ResourceManager;
};

template <typename T, typename Header = std::monostate>
class SharedBuffer: public SharedBufferBase {
public:
    SharedBuffer() = default;
    SharedBuffer(const SharedBuffer&) = delete;
    SharedBuffer& operator=(const SharedBuffer&) = delete;
    SharedBuffer(SharedBuffer&&) = default;
    SharedBuffer& operator=(SharedBuffer&&) = default;

private:
    static SharedBuffer<T, Header> create(const VkBufferUsageFlags& usage, size_t count) {
        SharedBuffer<T, Header> buffer;

        buffer.usage = usage;
        buffer.byteSize = sizeof(T) * count;
        if (!std::is_same_v<Header, std::monostate>) buffer.byteSize += sizeof(Header);
        return buffer;
    }

    friend class ResourceManager;
};
