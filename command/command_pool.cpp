#include "command_pool.hpp"

#include "../device/logical_device.hpp"
#include "../device/queue.hpp"
#include "../utils.hpp"

namespace VkSmol {

void CommandPool::init(const LogicalDevice& device, uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags) {
    VkCommandPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    createInfo.flags = flags;
    createInfo.queueFamilyIndex = queueFamilyIndex;

    vkCheck(vkCreateCommandPool(device.get(), &createInfo, nullptr, &rawHandle()), "Failed to create command pool");
}

void CommandPool::destroy(const LogicalDevice& device) {
    vkDestroyCommandPool(device.get(), rawHandle(), nullptr);
}

}
