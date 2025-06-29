#include "logicalDevice.hpp"

#include "./queue.hpp"
#include "./physicalDevice.hpp"
#include "./utils.hpp"

void LogicalDevice::init(
    PhysicalDevice physicalDevice,
    uint32_t queueFamilyIndex,
    std::vector<const char*> extensions,
    std::vector<const char*> validationLayers
) {
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
    queueCreateInfo.queueCount = 1;

    float queuePriority = 1.0f;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceFeatures deviceFeatures{};  // Everything is set to VK_FALSE for now

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    #ifndef NDEBUG
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    #else
        createInfo.enabledLayerCount = 0;
    #endif

    vkCheck(vkCreateDevice(physicalDevice.get(), &createInfo, nullptr, &device), "Failed to create logical device");
}

void LogicalDevice::destroy() {
    vkDestroyDevice(device, nullptr);
}