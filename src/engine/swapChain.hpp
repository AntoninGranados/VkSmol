#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>
#include <cstdint>
#include <limits>
#include <algorithm>

class PhysicalDevice;
class LogicalDevice;
class Surface;

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

SwapChainSupportDetails querySwapChainSupport(PhysicalDevice device, Surface surface);


class Swapchain {
public:
    void init(LogicalDevice device, PhysicalDevice physicalDevice, Surface surface, GLFWwindow* window);
    void destroy(LogicalDevice device);
    VkSwapchainKHR get() { return swapchain; };

    std::vector<VkImage> getImages() { return images; };
    std::vector<VkImageView> getImageViews() { return imageViews; };
    uint32_t getImageCount() { return imageCount; };
    VkFormat getImageFormat() { return imageFormat; };
    VkExtent2D getExtent() { return extent; };

private:
    VkSwapchainKHR swapchain;
    
    std::vector<VkImage> images;
    std::vector<VkImageView> imageViews;
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
