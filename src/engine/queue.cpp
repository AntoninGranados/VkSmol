#include "queue.hpp"

#include "./physicalDevice.hpp"
#include "./logicalDevice.hpp"

QueueFamilyIndices findQueueFamilies(PhysicalDevice physicalDevice) {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice.get(), &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice.get(), &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const VkQueueFamilyProperties& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
            break;
        }
        i++;
    }

    return indices;
}


void Queue::init(LogicalDevice device, uint32_t queueFamilyIndex) {
    vkGetDeviceQueue(device.get(), queueFamilyIndex, 0, &queue);
}
