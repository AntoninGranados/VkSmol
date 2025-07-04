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
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
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
    
    instance.init(appInfo, getRequiredExtensions(), validationLayers);

    debugMessenger.init(instance);

    surface.init(instance, window);

    std::function<int(PhysicalDevice)> scoring = [this] (PhysicalDevice device) { return scoreDevice(device); };
    physicalDevice.init(instance, scoring);
    
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice, surface);
    device.init(
        physicalDevice,
        queueFamilyIndices.getIndices(),
        deviceExtensions,
        validationLayers
    );

    graphicsQueue.init(device, queueFamilyIndices.graphicsFamily.value());
    presentQueue.init(device, queueFamilyIndices.presentFamily.value());

    swapchain.init(device, physicalDevice, surface, window);

    // Render Pass
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchain.getImageFormat();
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    renderPass.addAttachment(colorAttachment, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    renderPass.addSubpass(VK_PIPELINE_BIND_POINT_GRAPHICS, { 0 });
    renderPass.init(device);
    
    // Graphics Pipeline
    Shader vertShader, fragShader;
    vertShader.init(device, VK_SHADER_STAGE_VERTEX_BIT, "./shader/vert.glsl");
    fragShader.init(device, VK_SHADER_STAGE_FRAGMENT_BIT, "./shader/frag.glsl");

    graphicsPipeline.init(device, renderPass, { vertShader, fragShader });

    // TODO: should be delayed in case we want to rebuild the pipeline and reuse the shaders
    vertShader.destroy(device);
    fragShader.destroy(device);

    /*
    // TODO: make a wrapper for the viewport
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)swapchain.getExtent().width;
    viewport.height = (float)swapchain.getExtent().height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = swapchain.getExtent();
    */

    framebuffer.init(device, swapchain, renderPass);
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

int Application::scoreDevice(PhysicalDevice device) {
    // Check if all the  queue families are available
    QueueFamilyIndices indices = findQueueFamilies(device, surface);
    if (!indices.isComplete())
        return 0;

    // Check if all extensions are supported by the GPU
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device.get(), nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> supportedExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device.get(), nullptr, &extensionCount, supportedExtensions.data());

    for (const char* extension : deviceExtensions) {
        if (!std::any_of(
            supportedExtensions.begin(),
            supportedExtensions.end(),
            [&](VkExtensionProperties& properties) { return strcmp(extension, properties.extensionName) == 0; })
        )
            return 0;
    }

    // Check swap chain support
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device, surface);
    if (swapChainSupport.formats.empty() || swapChainSupport.presentModes.empty())
        return 0;
    
    return 1;
}



void Application::mainLoop() {
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }
}

void Application::cleanup() {
    framebuffer.destroy(device);

    graphicsPipeline.destroy(device);
    renderPass.destroy(device);

    swapchain.destroy(device);
    
    device.destroy();
    surface.destroy(instance);

    debugMessenger.destroy(instance);
    instance.destroy();

    glfwDestroyWindow(window);
    glfwTerminate();
}
