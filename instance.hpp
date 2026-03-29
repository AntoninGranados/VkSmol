#pragma once

#include <vulkan/vulkan.h>

#include <vector>

#include "unique_handle.hpp"

class Instance: public UniqueHandle<VkInstance, VK_NULL_HANDLE> {
public:
    void init(
        VkApplicationInfo appInfo,
        std::vector<const char*> extensions,
        std::vector<const char*> validationLayers
    );
    void destroy();

private:
    static void checkExtensionSupport(std::vector<const char*> extensions);
    static void checkValidationLayerSupport(std::vector<const char*> validationLayers);
};
