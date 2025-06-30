#include "surface.hpp"

#include "./instance.hpp"
#include "./utils.hpp"

void Surface::init(Instance instance, GLFWwindow* window) {
    vkCheck(glfwCreateWindowSurface(instance.get(), window, nullptr, &surface), "Failed to create window surface");
}

void Surface::destroy(Instance instance) {
    vkDestroySurfaceKHR(instance.get(), surface, nullptr);
}
