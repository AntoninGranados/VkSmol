#pragma once

#include <vulkan/vulkan.h>

#include <optional>
#include <vector>
#include <cstdint>

#include "../image/image.hpp"

namespace VkSmol {

class Device;
class PhysicalDevice;
class Surface;
class Semaphore;

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

SwapChainSupportDetails querySwapChainSupport(const PhysicalDevice& device, const Surface& surface);


class Swapchain {
public:
    Swapchain(Device& device, VkExtent2D requestedExtent);
    ~Swapchain();

    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;
    Swapchain(Swapchain&&) = delete;
    Swapchain& operator=(Swapchain&&) = delete;

    const VkSwapchainKHR& get() const { return swapchain; };

    // Returns std::nullopt if we need to recreate the swapchain
    std::optional<uint32_t> acquire(const Semaphore& signalSemaphore);
    // Returns true if we need to recreate the swapchain
    bool present(uint32_t imageIndex, const Semaphore& waitSemaphore);
    void recreate(VkExtent2D requestedExtent);

    Image& getImage(size_t i) { return images[i]; };
    ImageView& getImageView(size_t i) { return imageViews[i]; };

    uint32_t getImageCount() const { return imageCount; };
    VkFormat getImageFormat() const { return imageFormat; };
    const VkExtent2D& getExtent() const { return extent; };

private:
    Device& device;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<Image> images;
    std::vector<ImageView> imageViews;
    uint32_t imageCount = 0;
    VkFormat imageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D extent{};

    void create(VkExtent2D requestedExtent, VkSwapchainKHR oldSwapchain);

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, VkExtent2D requestedExtent);
};

}
