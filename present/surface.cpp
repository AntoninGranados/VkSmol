#include "surface.hpp"

#include "window.hpp"
#include "../instance/instance.hpp"
#include "../utils.hpp"

namespace VkSmol {

void Surface::init(const Instance& instance, const Window& window) {
    rawHandle() = window.createSurface(instance.get());
}

void Surface::destroy(const Instance& instance) {
    vkDestroySurfaceKHR(instance.get(), rawHandle(), nullptr);
    resetHandle();
}

}
