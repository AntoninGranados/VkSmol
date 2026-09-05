#include "logical_device.hpp"

#include <set>

#include "./queue.hpp"
#include "./physical_device.hpp"
#include "../utils.hpp"

void LogicalDevice::init(
    PhysicalDevice physicalDevice,
    std::vector<uint32_t> queueFamilyIndices,
    std::vector<const char*> extensions,
    [[maybe_unused]] std::vector<const char*> validationLayers
) {
    // Logic to avoid duplicate queue families
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies;
    for (uint32_t queueFamily : queueFamilyIndices)
        uniqueQueueFamilies.emplace(queueFamily);

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.emplace_back(queueCreateInfo);
    }

    //! When adding a feature, don't forget to add it to the `scoreDevice` function
    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    deviceFeatures.fragmentStoresAndAtomics = VK_TRUE;

    VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeature{};
    dynamicRenderingFeature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    dynamicRenderingFeature.dynamicRendering = VK_TRUE;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = &dynamicRenderingFeature;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    #ifndef NDEBUG
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    #else
        createInfo.enabledLayerCount = 0;
    #endif

    vkCheck(vkCreateDevice(physicalDevice.get(), &createInfo, nullptr, &rawHandle()), "Failed to create logical device");
}

void LogicalDevice::destroy() {
    vkDestroyDevice(rawHandle(), nullptr);
}

void LogicalDevice::waitIdle() const {
    vkDeviceWaitIdle(rawHandle());
}
