#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class PhysicalDevice;
class LogicalDevice;
class CommandBuffer;

class Buffer {
public:
    void init(LogicalDevice device, size_t size, VkBufferUsageFlags usage);
    void alloc(LogicalDevice device, PhysicalDevice physicalDevice);
    void destroy(LogicalDevice device);
    void free(LogicalDevice device);
    VkBuffer get() { return m_Buffer; };
    
    void fill(LogicalDevice device, void* data);

    // TODO: make a statice version to bind multiple buffer
    // TODO: remove error checking if NDBUG
    void bindVertex(CommandBuffer commandBuffer);

private:
    VkBuffer m_Buffer;
    VkDeviceMemory m_BufferMemory;
    size_t m_Size;
    VkBufferUsageFlags m_Usage;

    uint32_t findMemoryType(PhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);
};

