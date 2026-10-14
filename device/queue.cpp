#include "queue.hpp"

#include "../present/surface.hpp"
#include "./physical_device.hpp"
#include "./logical_device.hpp"
#include "../present/swapchain.hpp"
#include "../command/command_buffer.hpp"
#include "../sync/semaphore.hpp"
#include "../sync/fence.hpp"
#include "../utils.hpp"

namespace VkSmol {

QueueFamilyIndices findQueueFamilies(const PhysicalDevice& physicalDevice, const Surface& surface) {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice.get(), &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice.get(), &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const VkQueueFamilyProperties& queueFamily : queueFamilies) {
        // Graphics family
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            indices.graphicsFamily = i;
        
        // Present family
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice.get(), i, surface.get(), &presentSupport);
        if (presentSupport)
            indices.presentFamily = i;

        // Transfer family
        if (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT)
            indices.transferFamily = i;

        if (indices.isComplete()) break;
        i++;
    }

    return indices;
}

QueueFamilyIndices findQueueFamiliesHeadless(const PhysicalDevice& physicalDevice) {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice.get(), &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice.get(), &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const VkQueueFamilyProperties& queueFamily : queueFamilies) {
        // Graphics family
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            indices.graphicsFamily = i;

        // Transfer family
        if (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT)
            indices.transferFamily = i;

        if (indices.isComplete(true)) break;
        i++;
    }

    return indices;
}


void Queue::init(const LogicalDevice& device, uint32_t queueFamilyIndex) {
    vkGetDeviceQueue(device.get(), queueFamilyIndex, 0, &rawHandle());
    queueSubmit2 = device.functions().queueSubmit2;
}

void Queue::submit(
    const std::vector<const CommandBuffer*>& commandBuffers,
    const Fence& fence,
    const std::vector<const Semaphore*>& waitSemaphores, const std::vector<VkPipelineStageFlags2>& waitStages,
    const std::vector<const Semaphore*>& signalSemaphores
) {
    std::vector<VkSemaphoreSubmitInfo> waits, signals;
    for (size_t i = 0; i < waitSemaphores.size(); ++i)
        waits.push_back({
            .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = waitSemaphores[i]->get(),
            .stageMask = waitStages[i],
        });
    for (const Semaphore* signalSemaphore: signalSemaphores)
        signals.push_back({
            .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = signalSemaphore->get(),
            .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        });

    submit(commandBuffers, fence.get(), waits, signals);
}

void Queue::submit(const std::vector<const CommandBuffer*>& commandBuffers) {
    submit(commandBuffers, VK_NULL_HANDLE, {}, {});
}

void Queue::submit(
    const std::vector<const CommandBuffer*>& commandBuffers, VkFence fence,
    const std::vector<VkSemaphoreSubmitInfo>& waits, const std::vector<VkSemaphoreSubmitInfo>& signals
) {
    std::vector<VkCommandBufferSubmitInfo> commandBufferInfos;
    for (const CommandBuffer* commandBuffer: commandBuffers)
        commandBufferInfos.push_back({
            .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .commandBuffer = commandBuffer->get(),
        });

    const VkSubmitInfo2 submitInfo{
        .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount   = static_cast<uint32_t>(waits.size()),
        .pWaitSemaphoreInfos      = waits.data(),
        .commandBufferInfoCount   = static_cast<uint32_t>(commandBufferInfos.size()),
        .pCommandBufferInfos      = commandBufferInfos.data(),
        .signalSemaphoreInfoCount = static_cast<uint32_t>(signals.size()),
        .pSignalSemaphoreInfos    = signals.data(),
    };

    vkCheck(queueSubmit2(rawHandle(), 1, &submitInfo, fence), "Failed to submit queue");
}

bool Queue::present(
    const std::vector<const Swapchain*>& swapchains,
    const std::vector<uint32_t>& frameIndices,
    const std::vector<const Semaphore*>& waitSemaphores
) {
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    
    std::vector<VkSemaphore> waitVkSemaphore;
    for (const Semaphore* waitSemaphore: waitSemaphores)
        waitVkSemaphore.emplace_back(waitSemaphore->get());
    presentInfo.waitSemaphoreCount = static_cast<uint32_t>(waitVkSemaphore.size());
    presentInfo.pWaitSemaphores = waitVkSemaphore.data();

    std::vector<VkSwapchainKHR> vkSwapchain;
    for (const Swapchain* swapchain: swapchains)
        vkSwapchain.emplace_back(swapchain->get());
    presentInfo.swapchainCount = static_cast<uint32_t>(vkSwapchain.size());;
    presentInfo.pSwapchains = vkSwapchain.data();
    presentInfo.pImageIndices = frameIndices.data();

    VkResult result = vkQueuePresentKHR(rawHandle(), &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        return true;
    else if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to present the queue");

    return false;
}

void Queue::wait() {
    vkQueueWaitIdle(rawHandle());
}

}
