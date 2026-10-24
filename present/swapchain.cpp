#include "swapchain.hpp"

#include <algorithm>
#include <limits>
#include <memory>
#include <stdexcept>

#include "surface.hpp"
#include "../device/device.hpp"
#include "../sync/sync.hpp"
#include "../utils.hpp"

namespace VkSmol {

SwapChainSupportDetails querySwapChainSupport(const PhysicalDevice& device, const Surface& surface) {
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.get(), surface.get(), &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device.get(), surface.get(), &formatCount, nullptr);
    if (formatCount > 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device.get(), surface.get(), &formatCount, details.formats.data());
    }
    
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device.get(), surface.get(), &presentModeCount, nullptr);
    if (presentModeCount > 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device.get(), surface.get(), &presentModeCount, details.presentModes.data());
    }

    return details;
}


Swapchain::Swapchain(Device& device, VkExtent2D requestedExtent): device(device) {
    create(requestedExtent, VK_NULL_HANDLE);
}

Swapchain::~Swapchain() {
    const LogicalDevice& logicalDevice = device.getLogicalDevice();
    for (ImageView& view : imageViews)
        view.destroy(logicalDevice);
    vkDestroySwapchainKHR(logicalDevice.get(), swapchain, nullptr);
}

void Swapchain::create(VkExtent2D requestedExtent, VkSwapchainKHR oldSwapchain) {
    const LogicalDevice& logicalDevice = device.getLogicalDevice();
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device.getPhysicalDevice(), device.getSurface());

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    extent = chooseSwapExtent(swapChainSupport.capabilities, requestedExtent);
    imageFormat = surfaceFormat.format;

    imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount != 0)
        imageCount = std::min(imageCount, swapChainSupport.capabilities.maxImageCount);

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = device.getSurface().get();
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    QueueFamilyIndices indices = device.getQueueFamilies();
    // In the case where the two families are different
    // we have to tell the GPU that the images will be shared accross mulitple families
    std::vector<uint32_t> queueFamilyIndices = indices.getIndices();
    if (!indices.sameFamilies()) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = static_cast<uint32_t>(queueFamilyIndices.size());
        createInfo.pQueueFamilyIndices = queueFamilyIndices.data();
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;   // No added transformation
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;  // For an opaque window
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;   // Pixel behind other window will be clipped
    createInfo.oldSwapchain = oldSwapchain;

    vkCheck(vkCreateSwapchainKHR(logicalDevice.get(), &createInfo, nullptr, &swapchain), "Failed to create the swap chain");

    vkGetSwapchainImagesKHR(logicalDevice.get(), swapchain, &imageCount, nullptr);
    std::vector<VkImage> rawImages(imageCount);
    vkGetSwapchainImagesKHR(logicalDevice.get(), swapchain, &imageCount, rawImages.data());

    images = std::vector<Image>(imageCount);
    imageViews = std::vector<ImageView>(imageCount);
    for (size_t i = 0; i < imageCount; i++) {
        images[i].initExternal(rawImages[i], imageFormat, extent.width, extent.height);
        imageViews[i].init(logicalDevice, images[i]);
    }
}

void Swapchain::recreate(VkExtent2D requestedExtent) {
    VkSwapchainKHR oldSwapchain = swapchain;
    auto oldViews = std::make_shared<std::vector<ImageView>>(std::move(imageViews));
    device.retire([oldSwapchain, oldViews](const LogicalDevice& d) {
        for (ImageView& view : *oldViews) view.destroy(d);
        vkDestroySwapchainKHR(d.get(), oldSwapchain, nullptr);
    });

    create(requestedExtent, oldSwapchain);
}

std::optional<uint32_t> Swapchain::acquire(const Semaphore& signalSemaphore) {
    uint32_t imageIndex;

    VkResult result = vkAcquireNextImageKHR(device.getLogicalDevice().get(), swapchain, UINT64_MAX, signalSemaphore.get(), VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
        return std::nullopt;
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        throw std::runtime_error("Failed to acquire swapchain image");

    return imageIndex;
}

bool Swapchain::present(uint32_t imageIndex, const Semaphore& waitSemaphore) {
    return device.getPresentQueue().present({ this }, { imageIndex }, { &waitSemaphore });
}

VkSurfaceFormatKHR Swapchain::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    // Find if the SRGB non-linear color space and that BRGA (8bit) format are available
    for (const VkSurfaceFormatKHR& format : availableFormats) {
        if (format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR && format.format == VK_FORMAT_B8G8R8A8_SRGB)
            return format;
    }

    // Settle for the first one if our prefered color space/format are not available
    return availableFormats[0];
}

VkPresentModeKHR Swapchain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    // Find if the mailbox present mode is available
    // Mailbox : display the images as fast as possible while avoiding tearing
    for (const VkPresentModeKHR& presentMode : availablePresentModes) {
        if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            return presentMode;
    }

    // This mode is guaranted
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Swapchain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, VkExtent2D requestedExtent) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        return capabilities.currentExtent;

    VkExtent2D extent = requestedExtent;

    extent.width = std::clamp(
        extent.width,
        capabilities.minImageExtent.width,
        capabilities.maxImageExtent.width
    );
    extent.height = std::clamp(
        extent.height,
        capabilities.minImageExtent.height,
        capabilities.maxImageExtent.height
    );

    return extent;
}

}
