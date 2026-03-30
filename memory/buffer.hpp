#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "../tracked_resource.hpp"

class PhysicalDevice;
class LogicalDevice;
class CommandBuffer;
class CommandPool;
class Queue;

class Buffer: public TrackedResource<ResourceType::Buffer> {
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
    size_t getSize() { return size; };
    const size_t getSize() const { return size; };
    VkBufferUsageFlags getUsage() { return usage; };
    const VkBufferUsageFlags getUsage() const { return usage; };
    
    void fill(const LogicalDevice& device, void* data);
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
