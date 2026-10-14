#include "glfw_platform.hpp"

#include "imgui/imgui_impl_glfw.h"

#include <stdexcept>

namespace VkSmol {

GLFWPlatform::GLFWPlatform(const std::string& title, uint32_t width, uint32_t height) {
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

GLFWPlatform::~GLFWPlatform() {
    if (window) {
        glfwDestroyWindow(window);
        window = nullptr;
    }
    glfwTerminate();
}

std::vector<const char*> GLFWPlatform::getRequiredInstanceExtensions() const {
    uint32_t count = 0;
    const char** exts = glfwGetRequiredInstanceExtensions(&count);
    return std::vector<const char*>(exts, exts + count);
}

VkSurfaceKHR GLFWPlatform::createSurface(VkInstance instance) {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
        throw std::runtime_error("Failed to create window surface");
    return surface;
}

std::vector<const char*> GLFWPlatform::getRequiredDeviceExtensions() const {
    return { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
}

VkExtent2D GLFWPlatform::getExtent() const {
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    return { (uint32_t)w, (uint32_t)h };
}

bool GLFWPlatform::shouldClose() const {
    return glfwWindowShouldClose(window);
}

void GLFWPlatform::pollEvents() {
    glfwPollEvents();
}

void GLFWPlatform::waitEvents() {
    glfwWaitEvents();
}

void GLFWPlatform::pauseWhileMinimized() {
    int w = 0, h = 0;
    glfwGetFramebufferSize(window, &w, &h);
    while (w == 0 || h == 0) {
        glfwGetFramebufferSize(window, &w, &h);
        glfwWaitEvents();
    }
}

void GLFWPlatform::setResizeCallback(std::function<void(int, int)> cb) {
    resizeCallback = std::move(cb);
}

void GLFWPlatform::setCursorPosCallback(std::function<void(double, double)> cb) {
    cursorPosCallback = std::move(cb);
}

void GLFWPlatform::setScrollCallback(std::function<void(double, double)> cb) {
    scrollCallback = std::move(cb);
}

bool GLFWPlatform::getKey(int key) const {
    return glfwGetKey(window, key) == GLFW_PRESS;
}

bool GLFWPlatform::getMouseButton(int btn) const {
    return glfwGetMouseButton(window, btn) == GLFW_PRESS;
}

void GLFWPlatform::getCursorPos(double& x, double& y) const {
    glfwGetCursorPos(window, &x, &y);
}

void GLFWPlatform::getWindowSize(int& w, int& h) const {
    glfwGetWindowSize(window, &w, &h);
}

void GLFWPlatform::setCursorMode(int mode) {
    glfwSetInputMode(window, GLFW_CURSOR, mode);
}

double GLFWPlatform::getTime() const {
    return glfwGetTime();
}

void GLFWPlatform::imguiInitGlfwBackend() {
    ImGui_ImplGlfw_InitForVulkan(window, true);
}

void GLFWPlatform::imguiGlfwNewFrame() {
    ImGui_ImplGlfw_NewFrame();
}

void GLFWPlatform::imguiGlfwShutdown() {
    ImGui_ImplGlfw_Shutdown();
}

void GLFWPlatform::framebufferResizeBridge(GLFWwindow* w, int width, int height) {
    auto* self = static_cast<GLFWPlatform*>(glfwGetWindowUserPointer(w));
    if (self && self->resizeCallback) self->resizeCallback(width, height);
}

void GLFWPlatform::cursorPosBridge(GLFWwindow* w, double x, double y) {
    auto* self = static_cast<GLFWPlatform*>(glfwGetWindowUserPointer(w));
    if (self && self->cursorPosCallback) self->cursorPosCallback(x, y);
}

void GLFWPlatform::scrollBridge(GLFWwindow* w, double xoff, double yoff) {
    auto* self = static_cast<GLFWPlatform*>(glfwGetWindowUserPointer(w));
    if (self && self->scrollCallback) self->scrollCallback(xoff, yoff);
}

}
