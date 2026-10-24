#include "instance.hpp"

#include <algorithm>

#include "../utils.hpp"

namespace VkSmol {

void Instance::init(VkApplicationInfo appInfo, std::vector<const char*> extensions, bool validation) {
    const std::vector<const char*> validationLayers = {"VK_LAYER_KHRONOS_validation"};
    debugUtils = validation || isExtensionSupported(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    if (debugUtils) extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    Instance::checkExtensionSupport(extensions);
    if (validation) Instance::checkValidationLayerSupport(validationLayers);

    // Fill the creation info struct
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo{};
    if (validation) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
        DebugMessenger::populateCreateInfo(debugMessengerCreateInfo);
        createInfo.pNext = &debugMessengerCreateInfo;
    }
#ifdef __APPLE__
    createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    vkCheck(vkCreateInstance(&createInfo, nullptr, &rawHandle()), "Failed to create instance");
}

void Instance::destroy() {
    vkDestroyInstance(rawHandle(), nullptr);
}


bool Instance::isExtensionSupported(const char* extension) {
    // Find all the supported extensions
    uint32_t extensionCount;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> supportedExtensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, supportedExtensions.data());

    return std::any_of(
        supportedExtensions.begin(),
        supportedExtensions.end(),
        [&](VkExtensionProperties& properties) { return strcmp(extension, properties.extensionName) == 0; }
    );
}

void Instance::checkExtensionSupport(std::vector<const char*> extensions) {
    // Check if all extensions in `extensions` are supported
    for (const char* extension : extensions) {
        if (!isExtensionSupported(extension)) {
            char buf[MAX_MSG_SIZE];
            snprintf(buf, sizeof(buf), "Instance Extension [%s%s%s] is not supported", MSG_RED_B, extension, MSG_RESET);
            throw std::runtime_error(buf);
        }
    }
}

void Instance::checkValidationLayerSupport(std::vector<const char*> validationLayers) {
    // Find all the supported validation layers
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> supportedLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, supportedLayers.data());

    // Check if all validation layers in `validationLayers` are supported
    for (const char* layer : validationLayers) {
        if (!std::any_of(
            supportedLayers.begin(),
            supportedLayers.end(),
            [&](VkLayerProperties& properties) { return strcmp(layer, properties.layerName) == 0; })
        ) {
            char buf[MAX_MSG_SIZE];
            std::snprintf(buf, sizeof(buf), "Validation Layer [%s%s%s] is not supported", MSG_RED_B, layer, MSG_RESET);
            throw std::runtime_error(buf);
        }
    }
}

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
