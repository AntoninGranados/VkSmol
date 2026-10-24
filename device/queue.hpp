#pragma once

#include <vulkan/vulkan.h>

#include <optional>
#include <algorithm>
#include <vector>

namespace VkSmol {

class Surface;
class PhysicalDevice;
class LogicalDevice;
class CommandBuffer;
class Swapchain;
class Semaphore;
class Fence;

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily; // queue for graphics
    std::optional<uint32_t> transferFamily; // queue for transfering data from the CPU to the GPU
    std::optional<uint32_t> presentFamily;  // queue for presenting surface to the screen (not needed for headless mode)

    bool isComplete(bool headless = false) {
        return graphicsFamily.has_value() && transferFamily.has_value() && (headless || presentFamily.has_value());
    }

    // Check if all the families are the same
    bool sameFamilies() {
        return std::ranges::all_of(getIndices(), [&](uint32_t i) { return i == getIndices()[0]; });
    }

    std::vector<uint32_t> getIndices(bool headless = false) {
        std::vector<uint32_t> indices;
        indices.push_back(graphicsFamily.value());
        indices.push_back(transferFamily.value());
        if (!headless) indices.push_back(presentFamily.value());
        return indices;
    }
};

QueueFamilyIndices findQueueFamilies(const PhysicalDevice& physicalDevice, const Surface* surface);

class Queue {
public:
    VkQueue get() const { return queue; }

    void init(const LogicalDevice& device, uint32_t queueFamilyIndex);

    // Should be able to generate multiple VkSubmitInfo and submit them
    void submit(
        const std::vector<const CommandBuffer*>& commandBuffers,
        const Fence& fence,
        const std::vector<const Semaphore*>& waitSemaphores, const std::vector<VkPipelineStageFlags2>& waitStages,
        const std::vector<const Semaphore*>& signalSemaphores
    );
    void submit(const std::vector<const CommandBuffer*>& commandBuffers);
    // Returns true if we need to recreate the swapchain
    bool present(
        const std::vector<const Swapchain*>& swapchains,
        const std::vector<uint32_t>& frameIndices,
        const std::vector<const Semaphore*>& waitSemaphores
    );

    void wait();

private:
    VkQueue queue = VK_NULL_HANDLE;
    PFN_vkQueueSubmit2KHR queueSubmit2 = nullptr;

    void submit(const std::vector<const CommandBuffer*>& commandBuffers, VkFence fence,
                const std::vector<VkSemaphoreSubmitInfo>& waits, const std::vector<VkSemaphoreSubmitInfo>& signals);
};

}
