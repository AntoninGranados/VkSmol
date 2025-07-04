#include "commandBuffer.hpp"

#include "./commandPool.hpp"
#include "../logicalDevice.hpp"
#include "../utils.hpp"

void CommandBuffer::allocate(LogicalDevice device, CommandPool pool) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = pool.get();
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;  // Needs to be a component (variable)
    allocInfo.commandBufferCount = 1;   // Would need a static function to allocation multiple command buffers and return a std::vector

    vkCheck(vkAllocateCommandBuffers(device.get(), &allocInfo, &commandBuffer), "Failed to allocate command buffer");
}

void CommandBuffer::beginRecording() {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    vkCheck(vkBeginCommandBuffer(commandBuffer, &beginInfo), "Failed to begin recording command buffer");
}

void CommandBuffer::endRecording() {
    vkCheck(vkEndCommandBuffer(commandBuffer), "Failed to record command buffer");
}

void CommandBuffer::reset() {
    vkResetCommandBuffer(commandBuffer, 0);
}
