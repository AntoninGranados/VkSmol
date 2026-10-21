#include "semaphore.hpp"

#include "../device/logical_device.hpp"
#include "../utils.hpp"

namespace VkSmol {

void Semaphore::init(const LogicalDevice& device) {
    VkSemaphoreCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    vkCheck(vkCreateSemaphore(device.get(), &createInfo, nullptr, &rawHandle()), "Failed to create semaphore");
}

void Semaphore::destroy(const LogicalDevice& device) {
    if (isValid()) {
        vkDestroySemaphore(device.get(), rawHandle(), nullptr);
    }
    resetHandle();
}

}
