#pragma once

#include <vulkan/vulkan.h>

#include "../base/unique_handle.hpp"

namespace VkSmol {

class Window;
class Instance;

class Surface: public UniqueHandle<VkSurfaceKHR, VK_NULL_HANDLE> {
public:
    void init(const Instance& instance, const Window& window);
    void initFromRaw(VkSurfaceKHR handle) { rawHandle() = handle; }
    void destroy(const Instance& instance);
};

}
