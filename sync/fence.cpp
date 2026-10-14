#include "fence.hpp"

#include "../device/logical_device.hpp"
#include "../utils.hpp"

namespace VkSmol {

void Fence::init(const LogicalDevice& device, bool signaled) {
    VkFenceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (signaled)
        createInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    vkCheck(vkCreateFence(device.get(), &createInfo, nullptr, &rawHandle()), "Failed to create fence");
}

void Fence::destroy(const LogicalDevice& device) {
    vkDestroyFence(device.get(), rawHandle(), nullptr);
}

void Fence::wait(const std::vector<const Fence*>& fences, const LogicalDevice& device, uint64_t timeout) {
    std::vector<VkFence> vkFences;
    for (const Fence* fence : fences)
        vkFences.emplace_back(fence->get());
    vkWaitForFences(device.get(), static_cast<uint32_t>(vkFences.size()), vkFences.data(), VK_TRUE, timeout);
}

void Fence::wait(const LogicalDevice& device, uint64_t timeout) {
    vkWaitForFences(device.get(), 1, &rawHandle(), VK_TRUE, timeout);
}

void Fence::reset(const std::vector<const Fence*>& fences, const LogicalDevice& device) {
    std::vector<VkFence> vkFences;
    for (const Fence* fence : fences)
        vkFences.emplace_back(fence->get());
    vkResetFences(device.get(), static_cast<uint32_t>(vkFences.size()), vkFences.data());
}

void Fence::reset(const LogicalDevice& device) {
    vkResetFences(device.get(), 1, &rawHandle());
}

}
