#pragma once

#include "platform.hpp"

namespace VkSmol {

class HeadlessPlatform : public Platform {
public:
    HeadlessPlatform(uint32_t width, uint32_t height) : extent{ width, height } {}

    std::vector<const char*> getRequiredInstanceExtensions() const override { return {}; }
    VkSurfaceKHR             createSurface(VkInstance) override { return VK_NULL_HANDLE; }
    std::vector<const char*> getRequiredDeviceExtensions() const override { return {}; }
    bool                     isHeadless() const override { return true; }
    VkExtent2D               getExtent() const override { return extent; }
    void*                    getNativeWindowHandle() const override { return nullptr; }

    bool shouldClose() const override { return false; }
    void pollEvents() override {}
    void waitEvents() override {}
    void pauseWhileMinimized() override {}

    void setResizeCallback(std::function<void(int, int)>) override {}
    void setCursorPosCallback(std::function<void(double, double)>) override {}
    void setScrollCallback(std::function<void(double, double)>) override {}

    bool getKey(int) const override { return false; }
    bool getMouseButton(int) const override { return false; }
    void getCursorPos(double& x, double& y) const override { x = 0; y = 0; }
    void getWindowSize(int& w, int& h) const override { w = (int)extent.width; h = (int)extent.height; }
    void setCursorMode(int) override {}

    double getTime() const override { return 0.0; }

private:
    VkExtent2D extent;
};

}
