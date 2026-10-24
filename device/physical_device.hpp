#pragma once

#include <vulkan/vulkan.h>

#include <functional>

namespace VkSmol {

class Instance;

class PhysicalDevice {
public:
    PhysicalDevice(VkPhysicalDevice physicalDevice);
    PhysicalDevice();

    VkPhysicalDevice get() const { return physicalDevice; }

    void init(const Instance& instance);
    void init(const Instance& instance, std::function<int(const PhysicalDevice&)> scoringFunction);

private:
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

    static int scoreDeviceDefault(const PhysicalDevice& device);
};

}
