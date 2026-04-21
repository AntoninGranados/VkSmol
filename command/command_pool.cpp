#include "command_pool.hpp"

#include "../device/logical_device.hpp"
#include "../device/queue.hpp"
#include "../utils.hpp"

void CommandPool::init(const LogicalDevice& device, uint32_t queueFamilyIndex) {
    VkCommandPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    createInfo.queueFamilyIndex = queueFamilyIndex;

    vkCheck(vkCreateCommandPool(device.get(), &createInfo, nullptr, &rawHandle()), "Failed to create command pool");
}

void CommandPool::destroy(const LogicalDevice& device) {
    vkDestroyCommandPool(device.get(), rawHandle(), nullptr);
}
