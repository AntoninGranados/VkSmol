#pragma once

#include <vulkan/vulkan.h>
#include <functional>
#include <vector>

namespace VkSmol {

class Platform {
public:
    virtual ~Platform() = default;

    virtual std::vector<const char*> getRequiredInstanceExtensions() const = 0;

    // Returns VK_NULL_HANDLE for headless
    virtual VkSurfaceKHR createSurface(VkInstance instance) = 0;

    // Device extensions required by this platform (e.g. VK_KHR_swapchain)
    virtual std::vector<const char*> getRequiredDeviceExtensions() const = 0;

    virtual bool isHeadless() const = 0;

    // Offscreen extent (headless only; windowed reads from swapchain)
    virtual VkExtent2D getExtent() const = 0;

    // GLFWwindow* for GLFWPlatform, nullptr for headless
    virtual void* getNativeWindowHandle() const = 0;

    virtual bool shouldClose() const = 0;
    virtual void pollEvents() = 0;
    virtual void waitEvents() = 0;
    virtual void pauseWhileMinimized() = 0;

    virtual void setResizeCallback(std::function<void(int, int)> callback) = 0;
    virtual void setCursorPosCallback(std::function<void(double, double)> callback) = 0;
    virtual void setScrollCallback(std::function<void(double, double)> callback) = 0;

    virtual bool getKey(int key) const = 0;
    virtual bool getMouseButton(int btn) const = 0;
    virtual void getCursorPos(double& x, double& y) const = 0;
    virtual void getWindowSize(int& w, int& h) const = 0;
    virtual void setCursorMode(int mode) = 0;

    virtual double getTime() const = 0;
};

}
