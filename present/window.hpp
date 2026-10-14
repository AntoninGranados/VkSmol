#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <functional>
#include <string>
#include <vector>

namespace VkSmol {

class Window {
public:
    Window(const std::string& title, uint32_t width, uint32_t height);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    std::vector<const char*> getRequiredInstanceExtensions() const;
    VkSurfaceKHR             createSurface(VkInstance instance) const;
    VkExtent2D               getExtent() const;

    bool shouldClose() const;
    void pollEvents();
    void waitEvents();
    void pauseWhileMinimized();

    void setResizeCallback(std::function<void(int, int)> cb);
    void setCursorPosCallback(std::function<void(double, double)> cb);
    void setScrollCallback(std::function<void(double, double)> cb);

    bool getKey(int key) const;
    bool getMouseButton(int btn) const;
    void getCursorPos(double& x, double& y) const;
    void getWindowSize(int& w, int& h) const;
    void setCursorMode(int mode);

    double getTime() const;

    GLFWwindow* getWindow() const { return window; }

    void imguiInitGlfwBackend();
    void imguiGlfwNewFrame();
    void imguiGlfwShutdown();

private:
    GLFWwindow* window = nullptr;

    std::function<void(int, int)>       resizeCallback;
    std::function<void(double, double)> cursorPosCallback;
    std::function<void(double, double)> scrollCallback;

    static void framebufferResizeBridge(GLFWwindow* w, int width, int height);
    static void cursorPosBridge(GLFWwindow* w, double x, double y);
    static void scrollBridge(GLFWwindow* w, double xoff, double yoff);
};

}
