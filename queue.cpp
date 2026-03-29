#include "queue.hpp"

#include "./surface.hpp"
#include "./physical_device.hpp"
#include "./logical_device.hpp"
#include "./swapchain.hpp"
#include "./command/command_buffer.hpp"
#include "./sync/semaphore.hpp"
#include "./sync/fence.hpp"
#include "./utils.hpp"

// Search for the graphics queue and the present queue
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


void Queue::init(const LogicalDevice& device, uint32_t queueFamilyIndex) {
    vkGetDeviceQueue(device.get(), queueFamilyIndex, 0, &rawHandle());
}

void Queue::submit(
    const std::vector<const CommandBuffer*>& commandBuffers,
    const Fence& fence,
    const std::vector<const Semaphore*>& waitSemaphores, const std::vector<VkPipelineStageFlags>& waitStages,
    const std::vector<const Semaphore*>& signalSemaphores
) {
    std::vector<VkSemaphore> waitVkSemaphore, signalVkSemaphore;
    for (const Semaphore* waitSemaphore: waitSemaphores)
        waitVkSemaphore.emplace_back(waitSemaphore->get());
    for (const Semaphore* signalSemaphore: signalSemaphores)
        signalVkSemaphore.emplace_back(signalSemaphore->get());

    std::vector<VkCommandBuffer> vkCommandBuffers;
    for (const CommandBuffer* commandBuffer: commandBuffers)
        vkCommandBuffers.emplace_back(commandBuffer->get());

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = static_cast<uint32_t>(waitVkSemaphore.size());
    submitInfo.pWaitSemaphores = waitVkSemaphore.data();
    submitInfo.pWaitDstStageMask = waitStages.data();
    submitInfo.commandBufferCount = static_cast<uint32_t>(vkCommandBuffers.size());
    submitInfo.pCommandBuffers = vkCommandBuffers.data();
    submitInfo.signalSemaphoreCount = static_cast<uint32_t>(signalVkSemaphore.size());
    submitInfo.pSignalSemaphores = signalVkSemaphore.data();

    vkCheck(vkQueueSubmit(rawHandle(), 1, &submitInfo, fence.get()), "Failed to submit queue");
}

void Queue::submit(const std::vector<const CommandBuffer*>& commandBuffers) {
    std::vector<VkCommandBuffer> vkCommandBuffers;
    for (const CommandBuffer* commandBuffer: commandBuffers)
        vkCommandBuffers.emplace_back(commandBuffer->get());

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = static_cast<uint32_t>(vkCommandBuffers.size());
    submitInfo.pCommandBuffers = vkCommandBuffers.data();

    vkCheck(vkQueueSubmit(rawHandle(), 1, &submitInfo, VK_NULL_HANDLE), "Failed to submit queue");
}

uint32_t Queue::present(
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
        return RECREATE_SWAPCHAIN;
    else if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to present the queue");

    return 0;
}

void Queue::wait() {
    vkQueueWaitIdle(rawHandle());
}
