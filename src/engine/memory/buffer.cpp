#include "buffer.hpp"

#include "../physicalDevice.hpp"
#include "../logicalDevice.hpp"
#include "../command/commandBuffer.hpp"
#include "../utils.hpp"

void Buffer::init(LogicalDevice device, size_t size, VkBufferUsageFlags usage) {
    m_Size = size;
    m_Usage = usage;

    VkBufferCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.size = m_Size;
    createInfo.usage = m_Usage;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;     // Can only be used in one queue

    vkCheck(vkCreateBuffer(device.get(), &createInfo, nullptr, &m_Buffer), "Failed to create buffer");
}

void Buffer::alloc(LogicalDevice device, PhysicalDevice physicalDevice) {
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device.get(), m_Buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(
        physicalDevice,
        memRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    vkCheck(vkAllocateMemory(device.get(), &allocInfo, nullptr, &m_BufferMemory), "Failed to allocate buffer memory");

    vkBindBufferMemory(device.get(), m_Buffer, m_BufferMemory, 0);
}

void Buffer::destroy(LogicalDevice device) {
    vkDestroyBuffer(device.get(), m_Buffer, nullptr);
}

void Buffer::free(LogicalDevice device) {
    vkFreeMemory(device.get(), m_BufferMemory, nullptr);
}


void Buffer::fill(LogicalDevice device, void* data) {
    void* mem;
    vkMapMemory(device.get(), m_BufferMemory, 0, m_Size, 0, &mem);
    memcpy(mem, data, m_Size);
    vkUnmapMemory(device.get(), m_BufferMemory);
}

void Buffer::bindVertex(CommandBuffer commandBuffer) {
    if (m_Usage != VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
        throw std::runtime_error("Trying to bind a buffer as a vertex buffer when it is not");
    
    VkBuffer buffers[] = { m_Buffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer.get(), 0, 1, buffers, offsets);
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
