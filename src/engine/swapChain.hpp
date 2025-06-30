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

private:
    VkSwapchainKHR swapchain;

    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window);

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(
        const std::vector<VkSurfaceFormatKHR>& availableFormats
    );

    VkPresentModeKHR chooseSwapPresentMode(
        const std::vector<VkPresentModeKHR>& availablePresentModes
    );
};
