#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class PhysicalDevice;
class LogicalDevice;
class CommandBuffer;
class CommandPool;
class Queue;

class Buffer {
public:
    Buffer() = default;
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&& other);
    Buffer& operator=(Buffer&& other);

    // TODO: maybe combine init/alloc and destroye/free ?
    void init(
        LogicalDevice device,
        VkBufferUsageFlags usage,
        size_t size,
        bool persistentMapping = false
    );
    void initFilled(
        LogicalDevice device, PhysicalDevice physicalDevice,
        VkBufferUsageFlags usage,
        CommandPool transferCommandPool, Queue transferQueue,
        size_t size, void* data
    );

    void alloc(LogicalDevice device, PhysicalDevice physicalDevice, VkMemoryPropertyFlags properties);
    void free(LogicalDevice device);
    void destroy(LogicalDevice device);

    VkBuffer& get() { return buffer; };
    const VkBuffer& get() const { return buffer; };
    size_t getSize() { return size; };
    const size_t getSize() const { return size; };
    VkBufferUsageFlags getUsage() { return usage; };
    const VkBufferUsageFlags getUsage() const { return usage; };
    
    void fill(LogicalDevice device, void* data);
    void read(LogicalDevice device, void* out, size_t bytes, size_t offset = 0);
    static void copy(const Buffer& srcBuffer, const Buffer& dstBuffer, LogicalDevice device, CommandPool transferCommandPool, Queue transferQueue);

    // TODO: make a static version to bind multiple buffer
    void bindVertex(CommandBuffer commandBuffer);
    void bindIndex(CommandBuffer commandBuffer, VkIndexType indexType);

    static uint32_t findMemoryType(PhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

    bool persistentMapping = false;
private:
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory bufferMemory = VK_NULL_HANDLE;
    VkBufferUsageFlags usage = 0;
    size_t size = 0;

    void* bufferMapped = nullptr;
};
