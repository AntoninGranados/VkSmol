#pragma once

#include <vulkan/vulkan.h>

namespace VkSmol {

class PhysicalDevice;
class LogicalDevice;

using BufferAllocFlags = uint32_t;
enum BufferAllocFlagBits : BufferAllocFlags {
    BufferAllocMapped = 1 << 0,
};

class Buffer {
public:
    Buffer() = default;
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&& other);
    Buffer& operator=(Buffer&& other);

    void init(
        const LogicalDevice& device,
        PhysicalDevice physicalDevice,
        VkBufferUsageFlags usage,
        size_t size,
        VkMemoryPropertyFlags properties,
        bool persistentMapping = false
    );
    void destroy(const LogicalDevice& device);

    VkBuffer& get() { return buffer; };
    const VkBuffer& get() const { return buffer; };
    size_t getSize() const { return size; };
    VkBufferUsageFlags getUsage() const { return usage; };

    void write(const LogicalDevice& device, const void* data, size_t bytes, size_t offset = 0);
    void read(const LogicalDevice& device, void* out, size_t bytes, size_t offset = 0);

    static uint32_t findMemoryType(PhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

    bool persistentMapping = false;
private:
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory bufferMemory = VK_NULL_HANDLE;
    VkBufferUsageFlags usage = 0;
    size_t size = 0;

    void* bufferMapped = nullptr;
};

}
