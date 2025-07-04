#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>
#include <cstdint>
#include <limits>
#include <algorithm>

#include "./framebuffer.hpp"

class PhysicalDevice;
class LogicalDevice;
class Surface;
class RenderPass;
class Semaphore;

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

SwapChainSupportDetails querySwapChainSupport(PhysicalDevice device, Surface surface);


class Swapchain {
public:
    void init(LogicalDevice device, PhysicalDevice physicalDevice, Surface surface, GLFWwindow* window, std::optional<Swapchain> oldSwapChain = std::nullopt);
    void initFramebuffer(LogicalDevice device, RenderPass renderPass);
    void destroy(LogicalDevice device);
    // Does not currently support using the old swapchain
    // Recreate the swapchain (and its associated framebuffer)
    void recreate(LogicalDevice device, PhysicalDevice physicalDevice, Surface surface, GLFWwindow* window, RenderPass renderPass);
    VkSwapchainKHR get() { return swapchain; };
    Framebuffer getFramebuffer() { return framebuffer; };

    std::vector<VkImage> getImages() { return images; };
    VkImage getImage(size_t i) { return images[i]; };
    std::vector<VkImageView> getImageViews() { return imageViews; };
    VkImageView getImageView(size_t i) { return imageViews[i]; };
    uint32_t getImageCount() { return imageCount; };
    VkFormat getImageFormat() { return imageFormat; };
    VkExtent2D getExtent() { return extent; };

    // Returns RECREATE_SWAPCHAIN if we need to recreate the swapchain
    uint32_t getNextImageIndex(LogicalDevice device, Semaphore semaphore);

private:
    VkSwapchainKHR swapchain;
    
    std::vector<VkImage> images;
    std::vector<VkImageView> imageViews;
    Framebuffer framebuffer;
    uint32_t imageCount;
    VkFormat imageFormat;
    VkExtent2D extent;

    void initImages(LogicalDevice device);
    void initImageViews(LogicalDevice device);
    void destroyImageViews(LogicalDevice device);

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window);
};
