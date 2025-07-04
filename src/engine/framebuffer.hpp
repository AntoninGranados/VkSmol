#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>

class LogicalDevice;
class Swapchain;
class RenderPass;

// This is not a wrapper for VkFramebuffer, this represent the array of VkFramebuffer for a given Swapchain
class Framebuffer {
public:
    void init(LogicalDevice device, Swapchain swapchain, RenderPass renderPass);
    void destroy(LogicalDevice device);
    std::vector<VkFramebuffer> get() { return framebuffers; };
    VkFramebuffer getFrame(size_t i) { return framebuffers[i]; };

private:
    std::vector<VkFramebuffer> framebuffers;
};