#include "physical_device.hpp"

#include "../instance/instance.hpp"
#include "./queue.hpp"

#include <map>

namespace VkSmol {

PhysicalDevice::PhysicalDevice(VkPhysicalDevice physicalDevice) : physicalDevice(physicalDevice) {}
PhysicalDevice::PhysicalDevice() {}

void PhysicalDevice::init(const Instance& instance) {
    init(instance, PhysicalDevice::scoreDeviceDefault);
}

void PhysicalDevice::init(const Instance& instance, std::function<int(const PhysicalDevice&)> scoringFunction) {
    // Find all the GPUs connected
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance.get(), &deviceCount, nullptr);
    if (deviceCount == 0)
        throw std::runtime_error("Failed to locate a GPU with Vulkan support");
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance.get(), &deviceCount, devices.data());

    // Score all the GPUs
    std::multimap<int, VkPhysicalDevice> candidates;
    for (VkPhysicalDevice& device : devices) {
        int score = scoringFunction(PhysicalDevice(device));
        candidates.insert(std::make_pair(score, device));
    }
    
    // Select the highest ranked GPU
    if (candidates.rbegin()->first > 0)
        physicalDevice = candidates.rbegin()->second;
    else
        throw std::runtime_error("Failed to locate a suitable GPU");
}

// Scores every device with no difference
int PhysicalDevice::scoreDeviceDefault([[maybe_unused]] const PhysicalDevice& device) {
    // VkPhysicalDeviceProperties deviceProperties;
    // vkGetPhysicalDeviceProperties(device, &deviceProperties);
    // VkPhysicalDeviceFeatures deviceFeatures;
    // vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
    return 1;
}

}
