#pragma once

#include <vulkan/vulkan.h>

#include <vector>

#include "../base/unique_handle.hpp"

namespace VkSmol {

class PhysicalDevice;

struct DeviceFunctions {
    PFN_vkCmdBeginRenderingKHR   cmdBeginRendering   = nullptr;
    PFN_vkCmdEndRenderingKHR     cmdEndRendering     = nullptr;
    PFN_vkCmdPipelineBarrier2KHR cmdPipelineBarrier2 = nullptr;
    PFN_vkCmdWriteTimestamp2KHR  cmdWriteTimestamp2  = nullptr;
    PFN_vkQueueSubmit2KHR        queueSubmit2        = nullptr;
};

class LogicalDevice: public UniqueHandle<VkDevice, VK_NULL_HANDLE> {
public:
    void init(
        PhysicalDevice physicalDevice,
        std::vector<uint32_t> queueFamilyIndices,
        std::vector<const char*> extensions,
        std::vector<const char*> validationLayers
    );
    void destroy();

    void waitIdle() const;

    const DeviceFunctions& functions() const { return deviceFunctions; }

private:
    DeviceFunctions deviceFunctions;

    void loadFunctions();
};

}
