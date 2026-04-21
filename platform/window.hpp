#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <string>
#include <functional>

#include "../base/unique_handle.hpp"

class Window: public UniqueHandle<GLFWwindow*, nullptr> {
public:
    using FramebufferResizeFn = std::function<void(int, int)>;
    using CursorPosFn = std::function<void(double, double)>;
    using ScrollFn = std::function<void(double, double)>;

    void init(int width, int height, std::string name);
    void destroy();

    bool shouldClose();
    void pauseWhileMinimized();

    void setFrameBufferResizeCallback(FramebufferResizeFn callback) { onFramebufferResize = std::move(callback); }
    void setCursorPosCallback(CursorPosFn callback) { onCursorPos = std::move(callback); }
    void setScrollCallback(ScrollFn callback) { onScroll = std::move(callback); }

private:
    static void framebufferResizeBridge(GLFWwindow* window, int width, int height);
    static void cursorPosBridge(GLFWwindow* window, double x, double y);
    static void scrollBridge(GLFWwindow* window, double xoffset, double yoffset);

    FramebufferResizeFn onFramebufferResize;
    CursorPosFn onCursorPos;
    ScrollFn onScroll;
};
