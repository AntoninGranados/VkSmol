#pragma once

#include <vulkan/vulkan.h>

#include "unique_handle.hpp"

class Instance;

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