#include "logical_device.hpp"

#include <format>
#include <set>
#include <stdexcept>

#include "./queue.hpp"
#include "./physical_device.hpp"
#include "../utils.hpp"

namespace VkSmol {

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

    VkPhysicalDeviceSynchronization2Features synchronization2Feature{};
    synchronization2Feature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
    synchronization2Feature.synchronization2 = VK_TRUE;
    dynamicRenderingFeature.pNext = &synchronization2Feature;

    VkPhysicalDeviceShaderDrawParametersFeatures shaderDrawParametersFeature{};
    shaderDrawParametersFeature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES;
    shaderDrawParametersFeature.shaderDrawParameters = VK_TRUE;
    synchronization2Feature.pNext = &shaderDrawParametersFeature;

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
    loadFunctions();
}

template <typename T>
static T loadFunction(VkDevice device, const char* name) {
    T function = reinterpret_cast<T>(vkGetDeviceProcAddr(device, name));
    if (!function) throw std::runtime_error(std::format("Missing device function {}", name));
    return function;
}

void LogicalDevice::loadFunctions() {
    deviceFunctions = {
        .cmdBeginRendering   = loadFunction<PFN_vkCmdBeginRenderingKHR>(rawHandle(), "vkCmdBeginRenderingKHR"),
        .cmdEndRendering     = loadFunction<PFN_vkCmdEndRenderingKHR>(rawHandle(), "vkCmdEndRenderingKHR"),
        .cmdPipelineBarrier2 = loadFunction<PFN_vkCmdPipelineBarrier2KHR>(rawHandle(), "vkCmdPipelineBarrier2KHR"),
        .cmdWriteTimestamp2  = loadFunction<PFN_vkCmdWriteTimestamp2KHR>(rawHandle(), "vkCmdWriteTimestamp2KHR"),
        .queueSubmit2        = loadFunction<PFN_vkQueueSubmit2KHR>(rawHandle(), "vkQueueSubmit2KHR"),
    };
}

void LogicalDevice::destroy() {
    vkDestroyDevice(rawHandle(), nullptr);
    deviceFunctions = {};
}

void LogicalDevice::waitIdle() const {
    vkDeviceWaitIdle(rawHandle());
}

}
