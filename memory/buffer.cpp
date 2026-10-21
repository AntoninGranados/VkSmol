#include "buffer.hpp"

#include "../device/physical_device.hpp"
#include "../device/logical_device.hpp"
#include "../utils.hpp"

namespace VkSmol {

Buffer::Buffer(Buffer&& other)
    : TrackedResource(std::move(other)),
      persistentMapping(other.persistentMapping),
      buffer(other.buffer),
      bufferMemory(other.bufferMemory),
      usage(other.usage),
      size(other.size),
      bufferMapped(other.bufferMapped) {
    other.persistentMapping = false;
    other.buffer = VK_NULL_HANDLE;
    other.bufferMemory = VK_NULL_HANDLE;
    other.usage = 0;
    other.size = 0;
    other.bufferMapped = nullptr;
}

Buffer& Buffer::operator=(Buffer&& other) {
    if (this == &other) return *this;

    if (buffer != VK_NULL_HANDLE || bufferMemory != VK_NULL_HANDLE || bufferMapped != nullptr) {
        throw std::runtime_error("Buffer move assignment requires an empty destination");
    }

    TrackedResource::operator=(std::move(other));

    persistentMapping = other.persistentMapping;
    buffer = other.buffer;
    bufferMemory = other.bufferMemory;
    usage = other.usage;
    size = other.size;
    bufferMapped = other.bufferMapped;

    other.persistentMapping = false;
    other.buffer = VK_NULL_HANDLE;
    other.bufferMemory = VK_NULL_HANDLE;
    other.usage = 0;
    other.size = 0;
    other.bufferMapped = nullptr;

    return *this;
}

void Buffer::init(
    const LogicalDevice& device,
    VkBufferUsageFlags usage_,
    size_t size_,
    bool persistentMapping_
) {
    usage = usage_;
    size = size_;
    persistentMapping = persistentMapping_;

    VkBufferCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.size = size;
    createInfo.usage = usage;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;     //! Can only be used in one queue

    vkCheck(vkCreateBuffer(device.get(), &createInfo, nullptr, &buffer), "Failed to create buffer");
}

void Buffer::alloc(const LogicalDevice& device, PhysicalDevice physicalDevice, VkMemoryPropertyFlags properties) {
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device.get(), buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = Buffer::findMemoryType(
        physicalDevice,
        memRequirements.memoryTypeBits,
        properties
    );

    vkCheck(vkAllocateMemory(device.get(), &allocInfo, nullptr, &bufferMemory), "Failed to allocate buffer memory");

    vkBindBufferMemory(device.get(), buffer, bufferMemory, 0);

    if (persistentMapping)
        vkMapMemory(device.get(), bufferMemory, 0, size, 0, &bufferMapped);
}


void Buffer::free(const LogicalDevice& device) {
    if (persistentMapping && bufferMapped != nullptr) {
        vkUnmapMemory(device.get(), bufferMemory);
    }
    if (bufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device.get(), bufferMemory, nullptr);
    }
    bufferMemory = VK_NULL_HANDLE;
    bufferMapped = nullptr;
}

void Buffer::destroy(const LogicalDevice& device) {
    if (buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device.get(), buffer, nullptr);
    }
    buffer = VK_NULL_HANDLE;
    usage = 0;
    size = 0;
    persistentMapping = false;
}

void Buffer::write(const LogicalDevice& device, const void* data, size_t bytes, size_t offset) {
    if (offset + bytes > size)
        throw std::runtime_error("Buffer write out of range");

    if (persistentMapping) {
        memcpy(static_cast<char*>(bufferMapped) + offset, data, bytes);
    } else {
        void* mem;
        vkMapMemory(device.get(), bufferMemory, offset, bytes, 0, &mem);
        memcpy(mem, data, bytes);
        vkUnmapMemory(device.get(), bufferMemory);
    }
}

void Buffer::read(const LogicalDevice& device, void* out, size_t bytes, size_t offset) {
    if (offset + bytes > size)
        throw std::runtime_error("Buffer read out of range");

    if (persistentMapping) {
        memcpy(out, static_cast<char*>(bufferMapped) + offset, bytes);
    } else {
        void* mem;
        vkMapMemory(device.get(), bufferMemory, offset, bytes, 0, &mem);
        memcpy(out, mem, bytes);
        vkUnmapMemory(device.get(), bufferMemory);
    }
}

uint32_t Buffer::findMemoryType(PhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice.get(), &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if (typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }

    throw std::runtime_error("Failed to find suitable memory type");
}

}
