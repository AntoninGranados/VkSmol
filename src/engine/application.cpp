#include "application.hpp"

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const std::vector<const char*> instanceExtensions = {
#ifdef __APPLE__
    VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
#endif
#ifndef NDEBUG
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
};

const std::vector<const char*> deviceExtensions = {
    "VK_KHR_portability_subset",
};

// only used if NDEBUG is not defined
const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation",
};



void Application::run() {
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
}

/*
============== INITIALISATION ==============
*/

void Application::initWindow() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(WIDTH, HEIGHT, "Application", nullptr, nullptr);
    if (!window) throw std::runtime_error("Could not create GLFW window");
}

void Application::initVulkan() {
    // Application informations
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Application";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "VkSmol";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_4;
    
    instance.init(
        appInfo,
        getRequiredExtensions(),
        validationLayers
    );

    debugMessenger.init(instance);
    physicalDevice.init(instance);
    
    uint32_t queueFamilyIndex = findQueueFamilies(physicalDevice).graphicsFamily.value();
    device.init(
        physicalDevice,
        queueFamilyIndex,
        deviceExtensions,
        validationLayers
    );
    graphicsQueue.init(
        device,
        queueFamilyIndex
    );
}


std::vector<const char*> Application::getRequiredExtensions() {
    // Extensions needed by GLFW
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    // Extensions requiMSG_RED by the application
    std::vector<const char*> requiredExtensions;
    for (uint32_t i = 0; i < glfwExtensionCount; i++)
        requiredExtensions.emplace_back(glfwExtensions[i]);
    requiredExtensions.insert(requiredExtensions.end(), instanceExtensions.begin(), instanceExtensions.end());
    
    return requiredExtensions;
}

void Application::mainLoop() {
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }
}

void Application::cleanup() {
    device.destroy();
    debugMessenger.destroy(instance);
    instance.destroy();

    glfwDestroyWindow(window);
    glfwTerminate();
}
