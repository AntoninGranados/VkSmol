#pragma once

#include <vulkan/vulkan.h>

#include <GLFW/glfw3.h>

#include <vector>
#include <cstdint>

#include "../image/image.hpp"
#include "../image/image_view.hpp"

class Window;
class PhysicalDevice;
class LogicalDevice;
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
    Swapchain() = default;
    Swapchain(Swapchain&& other) noexcept;
    Swapchain& operator=(Swapchain&& other) noexcept;
    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    void init(const LogicalDevice& device, const PhysicalDevice& physicalDevice, const Surface& surface, const Window& window, const Swapchain* oldSwapChain = nullptr);
    void destroy(const LogicalDevice& device);
    VkSwapchainKHR& get() { return swapchain; };
    const VkSwapchainKHR& get() const { return swapchain; };

    Image& getImage(size_t i) { return images[i]; };
    ImageView& getImageView(size_t i) { return imageViews[i]; };
    
    uint32_t getImageCount() { return imageCount; };
    VkFormat* getImageFormat() { return &imageFormat; };
    
    VkExtent2D& getExtent() { return extent; };
    const VkExtent2D& getExtent() const { return extent; };

    // Returns SMOL_RECREATE_SWAPCHAIN if we need to recreate the swapchain
    uint32_t getNextImageIndex(const LogicalDevice& device, const Semaphore& semaphore);

private:
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;

    std::vector<Image> images;
    std::vector<ImageView> imageViews;
    uint32_t imageCount;
    VkFormat imageFormat;
    VkExtent2D extent;

    void initImages(const LogicalDevice& device);
    void initImageViews(const LogicalDevice& device);
    void destroyImageViews(const LogicalDevice& device);

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window);
};
