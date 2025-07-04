#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class Window;
class Instance;

class Surface {
public:
    void init(Instance instance, Window window);
    void destroy(Instance instance);
    VkSurfaceKHR get() { return surface; };

private:
    VkSurfaceKHR surface;
};
