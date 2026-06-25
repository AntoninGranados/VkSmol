#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "platform.hpp"

#include <string>
#include <functional>

class GLFWPlatform : public Platform {
public:
    GLFWPlatform(const std::string& title, uint32_t width, uint32_t height);
    ~GLFWPlatform() override;

    std::vector<const char*> getRequiredInstanceExtensions() const override;
    VkSurfaceKHR             createSurface(VkInstance instance) override;
    std::vector<const char*> getRequiredDeviceExtensions() const override;
    bool                     isHeadless() const override { return false; }
    VkExtent2D               getExtent() const override;
    void*                    getNativeWindowHandle() const override { return window; }

    bool shouldClose() const override;
    void pollEvents() override;
    void waitEvents() override;
    void pauseWhileMinimized() override;

    void setResizeCallback(std::function<void(int, int)> cb) override;
    void setCursorPosCallback(std::function<void(double, double)> cb) override;
    void setScrollCallback(std::function<void(double, double)> cb) override;

    bool getKey(int key) const override;
    bool getMouseButton(int btn) const override;
    void getCursorPos(double& x, double& y) const override;
    void getWindowSize(int& w, int& h) const override;
    void setCursorMode(int mode) override;

    double getTime() const override;

    void imguiInitGlfwBackend() override;
    void imguiGlfwNewFrame() override;
    void imguiGlfwShutdown() override;

private:
    GLFWwindow* window = nullptr;

    std::function<void(int, int)>       resizeCallback;
    std::function<void(double, double)> cursorPosCallback;
    std::function<void(double, double)> scrollCallback;

    static void framebufferResizeBridge(GLFWwindow* w, int width, int height);
    static void cursorPosBridge(GLFWwindow* w, double x, double y);
    static void scrollBridge(GLFWwindow* w, double xoff, double yoff);
};
