#include "window.hpp"

#include "imgui/imgui_impl_glfw.h"

#include <stdexcept>

namespace VkSmol {

Window::Window(const std::string& title, uint32_t width, uint32_t height) {
    if (!glfwInit()) throw std::runtime_error("Failed to initialize GLFW");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    window = glfwCreateWindow((int)width, (int)height, title.c_str(), nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeBridge);
    glfwSetCursorPosCallback(window, cursorPosBridge);
    glfwSetScrollCallback(window, scrollBridge);
}

Window::~Window() {
    if (window) {
        glfwDestroyWindow(window);
        window = nullptr;
    }
    glfwTerminate();
}

std::vector<const char*> Window::getRequiredInstanceExtensions() const {
    uint32_t count = 0;
    const char** exts = glfwGetRequiredInstanceExtensions(&count);
    return std::vector<const char*>(exts, exts + count);
}

VkSurfaceKHR Window::createSurface(VkInstance instance) const {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
        throw std::runtime_error("Failed to create window surface");
    return surface;
}

VkExtent2D Window::getExtent() const {
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    return { (uint32_t)w, (uint32_t)h };
}

bool Window::shouldClose() const {
    return glfwWindowShouldClose(window);
}

void Window::pollEvents() {
    glfwPollEvents();
}

void Window::waitEvents() {
    glfwWaitEvents();
}

void Window::pauseWhileMinimized() {
    int w = 0, h = 0;
    glfwGetFramebufferSize(window, &w, &h);
    while (w == 0 || h == 0) {
        glfwGetFramebufferSize(window, &w, &h);
        glfwWaitEvents();
    }
}

void Window::setResizeCallback(std::function<void(int, int)> cb) {
    resizeCallback = std::move(cb);
}

void Window::setCursorPosCallback(std::function<void(double, double)> cb) {
    cursorPosCallback = std::move(cb);
}

void Window::setScrollCallback(std::function<void(double, double)> cb) {
    scrollCallback = std::move(cb);
}

bool Window::getKey(int key) const {
    return glfwGetKey(window, key) == GLFW_PRESS;
}

bool Window::getMouseButton(int btn) const {
    return glfwGetMouseButton(window, btn) == GLFW_PRESS;
}

void Window::getCursorPos(double& x, double& y) const {
    glfwGetCursorPos(window, &x, &y);
}

void Window::getWindowSize(int& w, int& h) const {
    glfwGetWindowSize(window, &w, &h);
}

void Window::setCursorMode(int mode) {
    glfwSetInputMode(window, GLFW_CURSOR, mode);
}

double Window::getTime() const {
    return glfwGetTime();
}

void Window::imguiInitGlfwBackend() {
    ImGui_ImplGlfw_InitForVulkan(window, true);
}

void Window::imguiGlfwNewFrame() {
    ImGui_ImplGlfw_NewFrame();
}

void Window::imguiGlfwShutdown() {
    ImGui_ImplGlfw_Shutdown();
}

void Window::framebufferResizeBridge(GLFWwindow* w, int width, int height) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
    if (self && self->resizeCallback) self->resizeCallback(width, height);
}

void Window::cursorPosBridge(GLFWwindow* w, double x, double y) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
    if (self && self->cursorPosCallback) self->cursorPosCallback(x, y);
}

void Window::scrollBridge(GLFWwindow* w, double xoff, double yoff) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
    if (self && self->scrollCallback) self->scrollCallback(xoff, yoff);
}

}
