#include "logical_device.hpp"

#include <format>
#include <set>
#include <stdexcept>

#include "./device_features.hpp"
#include "./queue.hpp"
#include "./physical_device.hpp"
#include "../utils.hpp"

namespace VkSmol {

void LogicalDevice::init(
    PhysicalDevice physicalDevice, std::vector<uint32_t> queueFamilyIndices, const DeviceFeatureChain& features
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

    const std::vector<const char*> extensions = features.getExtensionNames();
    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = &features.getFeatures();
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

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
