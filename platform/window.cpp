#include "window.hpp"

namespace VkSmol {

Window::Window(Window&& other) noexcept
    : UniqueHandle(std::move(other)),
      onFramebufferResize(std::move(other.onFramebufferResize)),
      onCursorPos(std::move(other.onCursorPos)),
      onScroll(std::move(other.onScroll))
{
    if (isValid()) glfwSetWindowUserPointer(get(), this);
}

Window& Window::operator=(Window&& other) noexcept {
    UniqueHandle::operator=(std::move(other));
    onFramebufferResize = std::move(other.onFramebufferResize);
    onCursorPos         = std::move(other.onCursorPos);
    onScroll            = std::move(other.onScroll);
    if (isValid()) glfwSetWindowUserPointer(get(), this);
    return *this;
}

void Window::init(int width, int height, std::string name) {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    rawHandle() = glfwCreateWindow(width, height, name.c_str(), nullptr, nullptr);
    if (!isValid()) throw std::runtime_error("Could not create GLFW window");

    glfwSetWindowUserPointer(rawHandle(), this);

    glfwSetFramebufferSizeCallback(rawHandle(), &Window::framebufferResizeBridge);
    glfwSetCursorPosCallback(rawHandle(), &Window::cursorPosBridge);
    glfwSetScrollCallback(rawHandle(), &Window::scrollBridge);
}

void Window::destroy() {
    glfwDestroyWindow(rawHandle());
}

bool Window::shouldClose() {
    return glfwWindowShouldClose(rawHandle());
}

void Window::pauseWhileMinimized() {
    int width = 0, height = 0;
    glfwGetFramebufferSize(rawHandle(), &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(rawHandle(), &width, &height);
        glfwWaitEvents();
    }
}

// Private helpers
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

}
