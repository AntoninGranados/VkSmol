#include "window.hpp"

void Window::framebufferResizeBridge(GLFWwindow* glfwWindow, int width, int height) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(glfwWindow));
    if (self && self->onFramebufferResize) self->onFramebufferResize(width, height);
}

void Window::cursorPosBridge(GLFWwindow* glfwWindow, double x, double y) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(glfwWindow));
    if (self && self->onCursorPos) self->onCursorPos(x, y);
}

void Window::scrollBridge(GLFWwindow* glfwWindow, double xoffset, double yoffset) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(glfwWindow));
    if (self && self->onScroll) self->onScroll(xoffset, yoffset);
}

void Window::init(int width, int height, std::string name) {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    window = glfwCreateWindow(width, height, name.c_str(), nullptr, nullptr);
    if (!window) throw std::runtime_error("Could not create GLFW window");

    glfwSetWindowUserPointer(window, this);

    glfwSetFramebufferSizeCallback(window, &Window::framebufferResizeBridge);
    glfwSetCursorPosCallback(window, &Window::cursorPosBridge);
    glfwSetScrollCallback(window, &Window::scrollBridge);
}

void Window::destroy() {
    glfwDestroyWindow(window);
}

bool Window::shouldClose() {
    return glfwWindowShouldClose(window);
}

void Window::pauseWhileMinimized() {
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }
}
