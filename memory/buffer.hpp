#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "../base/tracked_resource.hpp"

namespace VkSmol {

class PhysicalDevice;
class LogicalDevice;
class CommandBuffer;
class CommandPool;
class Queue;

using BufferAllocFlags = uint32_t;
enum BufferAllocFlagBits : BufferAllocFlags {
    BufferAllocMapped = 1 << 0,
};

class Buffer: public TrackedResource {
public:
    Buffer() = default;
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&& other);
    Buffer& operator=(Buffer&& other);

    // TODO: maybe combine init/alloc and destroye/free ?
    void init(
        const LogicalDevice& device,
        VkBufferUsageFlags usage,
        size_t size,
        bool persistentMapping = false
    );
    void initFilled(
        const LogicalDevice& device, PhysicalDevice physicalDevice,
        VkBufferUsageFlags usage,
        const CommandPool& transferCommandPool, Queue transferQueue,
        size_t size, void* data
    );

    void alloc(const LogicalDevice& device, PhysicalDevice physicalDevice, VkMemoryPropertyFlags properties);
    void free(const LogicalDevice& device);
    void destroy(const LogicalDevice& device);

    VkBuffer& get() { return buffer; };
    const VkBuffer& get() const { return buffer; };
    size_t getSize() const { return size; };
    VkBufferUsageFlags getUsage() const { return usage; };
    
    void fill(const LogicalDevice& device, void* data);
    void write(const LogicalDevice& device, const void* data, size_t bytes, size_t offset = 0);
    void read(const LogicalDevice& device, void* out, size_t bytes, size_t offset = 0);
    static void copy(const Buffer& srcBuffer, const Buffer& dstBuffer, const LogicalDevice& device, const CommandPool& transferCommandPool, Queue transferQueue);

    // TODO: make a static version to bind multiple buffer
    void bindVertex(const CommandBuffer& commandBuffer);
    void bindIndex(const CommandBuffer& commandBuffer, VkIndexType indexType);

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
