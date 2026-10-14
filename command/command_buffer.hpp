#pragma once

#include <vulkan/vulkan.h>

#include "../base/unique_handle.hpp"

namespace VkSmol {

class LogicalDevice;
class CommandPool;
class Queue;

class CommandBuffer: public UniqueHandle<VkCommandBuffer, VK_NULL_HANDLE> {
public:
    void alloc(const LogicalDevice& device, const CommandPool& pool);
    void free(const LogicalDevice& device, const CommandPool& pool);

    void beginRecording(VkCommandBufferUsageFlagBits flags = (VkCommandBufferUsageFlagBits)0);
    void endRecording();
    void reset();

    static CommandBuffer beginSingleTimeCommands(const LogicalDevice& device, const CommandPool& pool);
    static void endSingleTimeCommands(const LogicalDevice& device, const CommandPool& pool, Queue queue, CommandBuffer& commandBuffer);
};

}
