#include "commandBuffer.hpp"

#include "./commandPool.hpp"
#include "../logicalDevice.hpp"
#include "../utils.hpp"

void CommandBuffer::alloc(LogicalDevice device, CommandPool pool) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = pool.get();
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;  // Needs to be a component (variable)
    allocInfo.commandBufferCount = 1;   // Would need a static function to allocation multiple command buffers and return a std::vector

    vkCheck(vkAllocateCommandBuffers(device.get(), &allocInfo, &m_CommandBuffer), "Failed to allocate command buffer");
}

void CommandBuffer::free(LogicalDevice device, CommandPool pool) {
    vkFreeCommandBuffers(device.get(), pool.get(), 1, &m_CommandBuffer);
}

void CommandBuffer::beginRecording(VkCommandBufferUsageFlagBits flags) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = flags;

    vkCheck(vkBeginCommandBuffer(m_CommandBuffer, &beginInfo), "Failed to begin recording command buffer");
}

void CommandBuffer::endRecording() {
    vkCheck(vkEndCommandBuffer(m_CommandBuffer), "Failed to record command buffer");
}

void CommandBuffer::reset() {
    vkResetCommandBuffer(m_CommandBuffer, 0);
}
