#include "swapChain.hpp"

#include "./physicalDevice.hpp"
#include "./surface.hpp"

#include <print>

SwapChainSupportDetails querySwapChainSupport(PhysicalDevice device, Surface surface) {
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