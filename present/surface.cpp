#include "surface.hpp"

#include "../platform/window.hpp"
#include "../instance/instance.hpp"
#include "../utils.hpp"

void Surface::init(const Instance& instance, const Window& window) {
    vkCheck(glfwCreateWindowSurface(instance.get(), window.get(), nullptr, &rawHandle()), "Failed to create window surface");
}

void Surface::destroy(const Instance& instance) {
    vkDestroySurfaceKHR(instance.get(), rawHandle(), nullptr);
}
