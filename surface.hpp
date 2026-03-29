#pragma once

#include <vulkan/vulkan.h>

#include "unique_handle.hpp"

class Window;
class Instance;

class Surface: public UniqueHandle<VkSurfaceKHR, VK_NULL_HANDLE> {
public:
    void init(const Instance& instance, const Window& window);
    void destroy(const Instance& instance);
};
