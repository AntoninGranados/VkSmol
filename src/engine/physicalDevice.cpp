#include "physicalDevice.hpp"

#include "./instance.hpp"
#include "./utils.hpp"

#include <map>

void PhysicalDevice::init(Instance instance) {
    init(instance, PhysicalDevice::scoreDeviceDefault);
}

void PhysicalDevice::init(Instance instance, std::function<int(VkPhysicalDevice)> scoringFunction) {
    // Find all the GPUs connected
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance.get(), &deviceCount, nullptr);
    if (deviceCount == 0)
        throw std::runtime_error("Failed to locate a GPU with Vulkan support");
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance.get(), &deviceCount, devices.data());

    // Score all the GPUs
    std::multimap<int, VkPhysicalDevice> candidates;
    for (const VkPhysicalDevice& device : devices) {
        int score = scoringFunction(device);
        candidates.insert(std::make_pair(score, device));
    }
    
    // Select the highest ranked GPU
    if (candidates.rbegin()->first > 0)
        physicalDevice = candidates.rbegin()->second;
    else
        throw std::runtime_error("Failed to locate a suitable GPU");
}

int PhysicalDevice::scoreDeviceDefault(VkPhysicalDevice device) {
    // VkPhysicalDeviceProperties deviceProperties;
    // vkGetPhysicalDeviceProperties(device, &deviceProperties);
    // VkPhysicalDeviceFeatures deviceFeatures;
    // vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
    return 1;
}