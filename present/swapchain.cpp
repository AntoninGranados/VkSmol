#include "swapchain.hpp"

#include "../platform/window.hpp"
#include "../device/physical_device.hpp"
#include "../device/logical_device.hpp"
#include "../device/queue.hpp"
#include "./surface.hpp"
#include "../sync/semaphore.hpp"
#include "../utils.hpp"

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


void Swapchain::init(const LogicalDevice& device, const PhysicalDevice& physicalDevice, const Surface& surface, const Window& window, const Swapchain* oldSwapChain) {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice, surface);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    extent = chooseSwapExtent(swapChainSupport.capabilities, window.get());
    imageFormat = surfaceFormat.format;

    imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount != 0)
        imageCount = std::min(imageCount, swapChainSupport.capabilities.maxImageCount);

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface.get();
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    QueueFamilyIndices indices = findQueueFamilies(physicalDevice, surface);
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
    createInfo.oldSwapchain = oldSwapChain == nullptr ? VK_NULL_HANDLE : oldSwapChain->get();

    vkCheck(vkCreateSwapchainKHR(device.get(), &createInfo, nullptr, &swapchain), "Failed to create the swap chain");

    initImages(device);
    initImageViews(device);
}

Swapchain::Swapchain(Swapchain&& other) noexcept:
    swapchain(std::exchange(other.swapchain, VK_NULL_HANDLE)),
    images(std::move(other.images)),
    imageViews(std::move(other.imageViews)),
    imageCount(other.imageCount),
    imageFormat(other.imageFormat),
    extent(other.extent)
{}

Swapchain& Swapchain::operator=(Swapchain&& other) noexcept {
    if (this != &other) {
        swapchain  = std::exchange(other.swapchain, VK_NULL_HANDLE);
        images     = std::move(other.images);
        imageViews = std::move(other.imageViews);
        imageCount = other.imageCount;
        imageFormat = other.imageFormat;
        extent     = other.extent;
    }
    return *this;
}

void Swapchain::destroy(const LogicalDevice& device) {
    if (swapchain == VK_NULL_HANDLE) return;
    destroyImageViews(device);
    vkDestroySwapchainKHR(device.get(), swapchain, nullptr);
    swapchain = VK_NULL_HANDLE;
    images.clear();
    imageViews.clear();
}

uint32_t Swapchain::getNextImageIndex(const LogicalDevice& device, const Semaphore& semaphore) {
    uint32_t frameIndex;

    VkResult result = vkAcquireNextImageKHR(device.get(), swapchain, UINT64_MAX, semaphore.get(), VK_NULL_HANDLE, &frameIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
        return RECREATE_SWAPCHAIN;
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        throw std::runtime_error("Failed to acquire swapchain image");

    return frameIndex;
}



void Swapchain::initImages(const LogicalDevice& device) {
    vkGetSwapchainImagesKHR(device.get(), swapchain, &imageCount, nullptr);
    std::vector<VkImage> rawImages(imageCount);
    vkGetSwapchainImagesKHR(device.get(), swapchain, &imageCount, rawImages.data());

    images.resize(imageCount);
    for (size_t i = 0; i < imageCount; i++) {
        images[i].initExternal(rawImages[i], imageFormat, extent.width, extent.height);
    }
}

void Swapchain::initImageViews(const LogicalDevice& device) {
    imageViews.resize(imageCount);

    for (size_t i = 0; i < imageCount; i++) {
        imageViews[i].init(device, images[i]);
    }
}

void Swapchain::destroyImageViews(const LogicalDevice& device) {
    for (ImageView& view : imageViews) {
        view.destroy(device);
    }
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

VkExtent2D Swapchain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        return capabilities.currentExtent;

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    VkExtent2D extent = {
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height)
    };

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
