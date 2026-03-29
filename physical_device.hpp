#pragma once

#include <vulkan/vulkan.h>

#include <functional>

#include "borrowed_handle.hpp"

class Instance;

class PhysicalDevice: public BorrowedHandle<VkPhysicalDevice, VK_NULL_HANDLE> {
public:
    PhysicalDevice(VkPhysicalDevice physicalDevice);
    PhysicalDevice();

    void init(const Instance& instance);
    void init(const Instance& instance, std::function<int(const PhysicalDevice&)> scoringFunction);

private:
    static int scoreDeviceDefault(const PhysicalDevice& device);
};