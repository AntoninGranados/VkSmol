#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <print>
#include <stdexcept>
#include <cstdlib>
#include <vector>

#include "./engine/instance.hpp"
#include "./engine/debugMessenger.hpp"
#include "./engine/physicalDevice.hpp"
#include "./engine/queue.hpp"
#include "./engine/logicalDevice.hpp"
#include "./engine/utils.hpp"

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


class Application {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    GLFWwindow* window;

    Instance instance;
    DebugMessenger debugMessenger;
    PhysicalDevice physicalDevice;
    LogicalDevice device;

    /*
    ============== INITIALISATION ==============
    */
    void initWindow() {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(WIDTH, HEIGHT, "Application", nullptr, nullptr);
        if (!window) throw std::runtime_error("Could not create GLFW window");
    }

    void initVulkan() {
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
        
        device.init(
            physicalDevice,
            deviceExtensions,
            validationLayers
        );
    }

    std::vector<const char*> getRequiredExtensions() {
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

    /*
    ============== RUN TIME ==============
    */

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
        }
    }

    /*
    ============== CLEAN UP ==============
    */

    void cleanup() {
        device.destroy();
        debugMessenger.destroy(instance);
        instance.destroy();

        glfwDestroyWindow(window);
        glfwTerminate();
    }
};

int main() {
#ifdef __APPLE__
    // Apple
#endif
#ifdef _WIN32
    // Windows
#endif
#ifdef __linux__
    // Linux
#endif

    Application app;

    try {
        app.run();
    } catch (const std::exception& e) {
        std::print("{}\n", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}