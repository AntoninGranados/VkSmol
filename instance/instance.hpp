#pragma once

#include <vulkan/vulkan.h>

#include <vector>

#include "../base/unique_handle.hpp"

namespace VkSmol {

class Instance: public UniqueHandle<VkInstance, VK_NULL_HANDLE> {
public:
    void init(VkApplicationInfo appInfo, std::vector<const char*> extensions, bool validation);
    void destroy();

    bool hasDebugUtils() const { return debugUtils; }

private:
    bool debugUtils = false;

    static bool isExtensionSupported(const char* extension);
    static void checkExtensionSupport(std::vector<const char*> extensions);
    static void checkValidationLayerSupport(std::vector<const char*> validationLayers);
};

class DebugMessenger: public UniqueHandle<VkDebugUtilsMessengerEXT, VK_NULL_HANDLE> {
public:
    void init(const Instance& instance);
    void destroy(const Instance& instance);

    static void populateCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

private:
    static VKAPI_ATTR VkBool32 VKAPI_CALL callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData
    );

    // We need this proxy function because we have to load `vkCreateDebugUtilsMessengerEXT` manually
    static VkResult CreateDebugUtilsMessengerEXT(
        const Instance& instance,
        const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkDebugUtilsMessengerEXT* pDebugMessenger
    );

    static void DestroyDebugUtilsMessengerEXT(const Instance& instance,
        VkDebugUtilsMessengerEXT debugMessenger, const
        VkAllocationCallbacks* pAllocator
    );
};

}
