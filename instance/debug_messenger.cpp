#include "debug_messenger.hpp"

#include "./instance.hpp"
#include "../utils.hpp"

namespace VkSmol {

void DebugMessenger::init(const Instance& instance) {
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    populateCreateInfo(createInfo);
    vkCheck(CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &rawHandle()), "Failed to set up debug messenger");
}

void DebugMessenger::destroy(const Instance& instance) {
    if (isValid()) {
        DestroyDebugUtilsMessengerEXT(instance, rawHandle(), nullptr);
    }
    resetHandle();
}

void DebugMessenger::populateCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = callback;
}


VKAPI_ATTR VkBool32 VKAPI_CALL DebugMessenger::callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    [[maybe_unused]] void* pUserData
) {
    // Log the message based on it's severity
    switch (messageSeverity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT :    std::fprintf(stderr, "[ERROR] %s\n", pCallbackData->pMessage); break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT :  std::fprintf(stderr, "[WARNING] %s\n", pCallbackData->pMessage); break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT :  std::fprintf(stdout, "[VERBOSE] %s\n", pCallbackData->pMessage); break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT :     std::fprintf(stdout, "[INFO] %s\n", pCallbackData->pMessage); break;
        default: break;
    }
    return VK_FALSE;
}

// Find the `vkCreateDebugUtilsMessengerEXT` function
VkResult DebugMessenger::CreateDebugUtilsMessengerEXT(
    const Instance& instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger
) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance.get(), "vkCreateDebugUtilsMessengerEXT");
    if (func == nullptr)
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    return func(instance.get(), pCreateInfo, pAllocator, pDebugMessenger);
}

// Find the `vkDestroyDebugUtilsMessengerEXT` function
void DebugMessenger::DestroyDebugUtilsMessengerEXT(
    const Instance& instance,
    VkDebugUtilsMessengerEXT debugMessenger, const
    VkAllocationCallbacks* pAllocator
) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance.get(), "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr)
        func(instance.get(), debugMessenger, pAllocator);
}

}
