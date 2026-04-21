#pragma once

#include <vulkan/vulkan.h>

#include <optional>
#include <algorithm>
#include <vector>

#include "../base/borrowed_handle.hpp"

class Surface;
class PhysicalDevice;
class LogicalDevice;
class CommandBuffer;
class Swapchain;
class Semaphore;
class Fence;

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily; // queue for graphics
    std::optional<uint32_t> presentFamily;  // queue for prensenting surface to the screen
    std::optional<uint32_t> transferFamily; // queue for transfering data from the CPU to the GPU

    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value() && transferFamily.has_value();
    }

    // Check if all the families are the same
    bool sameFamilies() {
        return std::ranges::all_of(getIndices(), [&](int i) { return i == getIndices()[0]; });
    }

    std::vector<uint32_t> getIndices() {
        return std::vector<uint32_t> { graphicsFamily.value(), presentFamily.value(), transferFamily.value() };
    }
};

QueueFamilyIndices findQueueFamilies(const PhysicalDevice& physicalDevice, const Surface& surface);

class Queue: public BorrowedHandle<VkQueue, VK_NULL_HANDLE> {
public:
    void init(const LogicalDevice& device, uint32_t queueFamilyIndex);

    // Should be able to generate multiple VkSubmitInfo and submit them
    void submit(
        const std::vector<const CommandBuffer*>& commandBuffers,
        const Fence& fence,
        const std::vector<const Semaphore*>& waitSemaphores, const std::vector<VkPipelineStageFlags>& waitStages,
        const std::vector<const Semaphore*>& signalSemaphores
    );
    void submit(const std::vector<const CommandBuffer*>& commandBuffers);
    // Returns RECREATE_SWAPCHAIN if we need to recreate the swapchain
    uint32_t present(
        const std::vector<const Swapchain*>& swapchains,
        const std::vector<uint32_t>& frameIndices,
        const std::vector<const Semaphore*>& waitSemaphores
    );

    void wait();
};
