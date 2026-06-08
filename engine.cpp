#include "engine.hpp"

#include <algorithm>
#include <vector>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_vulkan.h"

// TODO: get this values from the application (and pass them in the engine.init function)
const uint32_t WIDTH = 1280;
const uint32_t HEIGHT = 720;

const std::vector<const char*> instanceExtensions = {
#ifdef __APPLE__
    VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
#ifndef NDEBUG
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
};

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
#ifdef __APPLE__
    "VK_KHR_portability_subset",
#endif
};

// only used if NDEBUG is ndef
const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation",
};

// Public
void VkSmol::init(std::string appName, uint32_t appVersion) {
    glfwInit();

    window.setFrameBufferResizeCallback([this](int, int) {
        framebufferResized = true;
    });

    window.init(WIDTH, HEIGHT, appName.c_str());

    // VkSmol informations
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = appName.c_str();
    appInfo.applicationVersion = appVersion;
    appInfo.pEngineName = "VkSmol";
    appInfo.engineVersion = VK_MAKE_API_VERSION(0, 0, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_4;

    instance.init(appInfo, getRequiredExtensions(), validationLayers);

    debugMessenger.init(instance);

    surface.init(instance, window);

    std::function<int(PhysicalDevice)> scoring = [this](PhysicalDevice device) { return scoreDevice(device); };
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
    transferQueue.init(device, queueFamilyIndices.transferFamily.value());

    swapchain.init(device, physicalDevice, surface, window);

    dynamicRenderer.init(device);

    graphicsCommandPool.init(device, queueFamilyIndices.graphicsFamily.value());
    transferCommandPool.init(device, queueFamilyIndices.transferFamily.value());
    commandBuffers.resize(MAX_FRAME_IN_FLIGHT);
    for (CommandBuffer& commandBuffer : commandBuffers)
        commandBuffer.alloc(device, graphicsCommandPool);

    initImgui();

    // Synchronization Objects
    {
        imageAvailableSemaphores.resize(MAX_FRAME_IN_FLIGHT);
        renderFinishedSemaphores.resize(MAX_FRAME_IN_FLIGHT);
        uiFinishedSemaphores.resize(MAX_FRAME_IN_FLIGHT);
        renderInFlightFences.resize(MAX_FRAME_IN_FLIGHT);
        uiInFlightFences.resize(MAX_FRAME_IN_FLIGHT);
        for (size_t i = 0; i < MAX_FRAME_IN_FLIGHT; i++) {
            imageAvailableSemaphores[i].init(device);

            renderFinishedSemaphores[i].resize(swapchain.getImageCount());
            for (Semaphore& semaphore : renderFinishedSemaphores[i])
                semaphore.init(device);

            uiFinishedSemaphores[i].resize(swapchain.getImageCount());
            for (Semaphore& semaphore : uiFinishedSemaphores[i])
                semaphore.init(device);

            renderInFlightFences[i].init(device, true);
            uiInFlightFences[i].init(device, true);
        }
    }

    resourceManager.init({
        .device = &device,
        .physicalDevice = &physicalDevice,
        .transferCommandPool = &transferCommandPool,
        .transferQueue = &transferQueue,
    });
}

std::optional<FrameContext> VkSmol::beginFrame() {
    Fence::wait({ &renderInFlightFences[currentFrame], &uiInFlightFences[currentFrame] }, device);

    resourceManager.collectFrame(currentFrame);
    resourceManager.setRetirementFrame(currentFrame);

    const uint32_t result = swapchain.getNextImageIndex(device, imageAvailableSemaphores[currentFrame]);
    if (result == RECREATE_SWAPCHAIN) {
        recreateSwapchain();
        return {};
    }

    Fence::reset({ &renderInFlightFences[currentFrame], &uiInFlightFences[currentFrame] }, device);
    imageIndex = result;

    return FrameContext{
        .extent = getExtent(),
        .currentFrame = currentFrame,
        .imageIndex = imageIndex,
        .swapchainGeneration = swapchainGeneration,
    };
}

void VkSmol::endFrame() {
    const uint32_t result = presentQueue.present({ &swapchain }, { imageIndex }, { &uiFinishedSemaphores[currentFrame][imageIndex] });
    if (result == RECREATE_SWAPCHAIN || framebufferResized) {
        recreateSwapchain();
    }

    currentFrame = (currentFrame + 1) % MAX_FRAME_IN_FLIGHT;
}

CommandBuffer& VkSmol::beginRecordingRender() {
    commandBuffers[currentFrame].reset();

    commandBuffers[currentFrame].beginRecording();
    return commandBuffers[currentFrame];
}

void VkSmol::endRecoringRender(CommandBuffer& commandBuffer) {
    commandBuffer.endRecording();

    graphicsQueue.submit(
        { &commandBuffers[currentFrame] },
        renderInFlightFences[currentFrame],
        { &imageAvailableSemaphores[currentFrame] }, { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT },
        { &renderFinishedSemaphores[currentFrame][imageIndex] }
    );
}

CommandBuffer& VkSmol::beginRecordingUiRender() {
    uiCommandBuffers[currentFrame].reset();
    uiCommandBuffers[currentFrame].beginRecording();
    return uiCommandBuffers[currentFrame];
}

void VkSmol::endRecoringUiRender(CommandBuffer& commandBuffer) {
    commandBuffer.endRecording();
    graphicsQueue.submit(
        { &uiCommandBuffers[currentFrame] },
        uiInFlightFences[currentFrame],
        { &renderFinishedSemaphores[currentFrame][imageIndex] }, { VK_PIPELINE_STAGE_ALL_COMMANDS_BIT },
        { &uiFinishedSemaphores[currentFrame][imageIndex] }
    );
}

void VkSmol::waitIdle() {
    device.waitIdle();

    resourceManager.flushAll();
}

void VkSmol::terminate() {
    device.waitIdle();

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    resourceManager.flushAll();
    resourceManager.terminate();

    uiDescriptorPool.destroy(device);

    for (size_t i = 0; i < MAX_FRAME_IN_FLIGHT; i++) {
        imageAvailableSemaphores[i].destroy(device);
        for (Semaphore& semaphore : renderFinishedSemaphores[i])
            semaphore.destroy(device);
        for (Semaphore& semaphore : uiFinishedSemaphores[i])
            semaphore.destroy(device);
        renderInFlightFences[i].destroy(device);
        uiInFlightFences[i].destroy(device);
    }

    for (CommandBuffer& commandBuffer : commandBuffers)
        commandBuffer.free(device, graphicsCommandPool);
    for (CommandBuffer& commandBuffer : uiCommandBuffers)
        commandBuffer.free(device, uiCommandPool);
    graphicsCommandPool.destroy(device);
    transferCommandPool.destroy(device);
    uiCommandPool.destroy(device);

    swapchain.destroy(device);

    device.destroy();
    surface.destroy(instance);

    debugMessenger.destroy(instance);
    instance.destroy();

    window.destroy();
    glfwTerminate();
}

GraphicsPipeline VkSmol::initGraphicsPipeline(
    VkPipelineVertexInputStateCreateInfo vertexInputInfo,
    const std::vector<const Shader*>& shaders,
    const std::vector<const DescriptorSetLayout*>& setLayouts,
    GraphicsPipeline other,
    VkFormat colorFormat
) {
    // Dynamic Rendering Info
    VkFormat format = colorFormat == VK_FORMAT_UNDEFINED ? *swapchain.getImageFormat() : colorFormat;
    VkPipelineRenderingCreateInfoKHR pipelineCreate{};
    pipelineCreate.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    pipelineCreate.colorAttachmentCount    = 1;
    pipelineCreate.pColorAttachmentFormats = &format;

    GraphicsPipeline pipeline;
    pipeline.init(
        device,
        pipelineCreate,
        vertexInputInfo,
        shaders,
        setLayouts,
        other
    );
    return pipeline;
}

void VkSmol::destroyGraphicsPipeline(GraphicsPipeline& pipeline) {
    pipeline.destroy(device);
}

Shader VkSmol::createShader(VkShaderStageFlagBits stage, std::string filepath) {
    Shader shader;
    shader.init(device, stage, filepath);
    return shader;
}

void VkSmol::destroyShader(Shader& shader) {
    shader.destroy(device);
}

void VkSmol::initDescriptorSetLayout(DescriptorSetLayout& descriptorSetLayout) {
    resourceManager.initDescriptorSetLayout(descriptorSetLayout);
}

void VkSmol::destroyDescriptorSetLayout(DescriptorSetLayout& descriptorSetLayout) {
    resourceManager.destroyDescriptorSetLayout(descriptorSetLayout);
}

DescriptorSetGroup VkSmol::createDescriptorSetGroup(DescriptorSetLayout& layout, std::string name) {
    return resourceManager.createDescriptorSetGroup(layout, name);
}

void VkSmol::updateDescriptorSetGroup(DescriptorSetGroup& group, uint32_t frameIndex, const DescriptorWriter& writer) {
    resourceManager.updateDescriptorSetGroup(group, frameIndex, writer);
}

void VkSmol::destroyDescriptorSetGroup(DescriptorSetGroup& group) {
    resourceManager.destroyDescriptorSetGroup(group);
}

Buffer VkSmol::createBuffer(VkBufferUsageFlags usage, size_t size, void* data, std::string name) {
    return resourceManager.createBuffer(usage, size, data, name);
}

Buffer VkSmol::createReadbackBuffer(size_t size, bool persistentMapping, std::string name) {
    return resourceManager.createReadbackBuffer(size, persistentMapping, name);
}

void VkSmol::fillBuffer(Buffer& buffer, void* data) {
    resourceManager.fillBuffer(buffer, data);
}

void VkSmol::readBuffer(Buffer& buffer, void* out, size_t bytes, size_t offset) {
    resourceManager.readBuffer(buffer, out, bytes, offset);
}

void VkSmol::destroyBuffer(Buffer& buffer) {
    resourceManager.destroyBuffer(buffer);
}

Image VkSmol::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, std::string name) {
    return resourceManager.createImage(width, height, format, usage, properties, name);
}

Image VkSmol::createImageFromPath(std::string path, std::string name) {
    return resourceManager.createImageFromPath(path, name);
}

void VkSmol::destroyImage(Image& image) {
    resourceManager.destroyImage(image);
}

ImageView VkSmol::createImageView(Image& image, std::string name) {
    return resourceManager.createImageView(image);
}

void VkSmol::destroyImageView(ImageView& imageView) {
    resourceManager.destroyImageView(imageView);
}

Sampler VkSmol::createSampler(std::string name) {
    return resourceManager.createSampler();
}

void VkSmol::destroySampler(Sampler& sampler) {
    resourceManager.destroySampler(sampler);
}

//! The render area is fixed to the screen size for now
void VkSmol::beginDynamicRenderer(
    const CommandBuffer& commandBuffer,
    VkImageView imageView, VkImageLayout imageLayout,
    VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp,
    VkClearColorValue clearColor
) {
    dynamicRenderer.begin(
        commandBuffer,
        imageView, imageLayout,
        loadOp, storeOp,
        clearColor, VkRect2D { VkOffset2D {}, swapchain.getExtent() }
    );
}

void VkSmol::endDynamicRenderer(const CommandBuffer& commandBuffer) {
    dynamicRenderer.end(commandBuffer);
}

void VkSmol::barrier(
    const CommandBuffer& commandBuffer,
    VkImage image,
    VkImageLayout srcLayout, VkImageLayout dstLayout,
    VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
    VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask
) {
    ImageBarrier barrier;
    barrier.call(
        commandBuffer,
        image,
        srcLayout, dstLayout,
        srcAccessMask, dstAccessMask,
        srcStageMask, dstStageMask
    );
}

bool VkSmol::shouldTerminate() {
    return window.shouldClose();
}

Image& VkSmol::getSwapchainImage(size_t imageIndex) {
    return swapchain.getImage(imageIndex);
}

ImageView& VkSmol::getSwapchainImageView(size_t imageIndex) {
    return swapchain.getImageView(imageIndex);
}

// Private helpers
void VkSmol::initImgui() {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice, surface);

    uiCommandPool.init(device, indices.graphicsFamily.value());
    uiCommandBuffers.resize(MAX_FRAME_IN_FLIGHT);
    for (CommandBuffer& commandBuffer : uiCommandBuffers)
        commandBuffer.alloc(device, uiCommandPool);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_WindowBg]       = ImVec4(0.0f, 0.0f, 0.0f, 0.30f); // keep docked windows transparent
    // style.Colors[ImGuiCol_ChildBg]        = ImVec4(0.0f, 0.0f, 0.0f, 0.30f);
    style.Colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    ImGui_ImplGlfw_InitForVulkan(window.get(), true);
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance       = instance.get();
    initInfo.PhysicalDevice = physicalDevice.get();
    initInfo.Device         = device.get();
    initInfo.QueueFamily    = indices.graphicsFamily.value();
    initInfo.Queue          = graphicsQueue.get();

    uiDescriptorPool.addPoolSize(VK_DESCRIPTOR_TYPE_SAMPLER, 1000);
    uiDescriptorPool.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000);
    uiDescriptorPool.addPoolSize(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000);
    uiDescriptorPool.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000);
    uiDescriptorPool.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000);
    uiDescriptorPool.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000);
    uiDescriptorPool.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000);
    uiDescriptorPool.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000);
    uiDescriptorPool.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000);
    uiDescriptorPool.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000);
    uiDescriptorPool.addPoolSize(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 100);
    uiDescriptorPool.init(device);

    // initInfo.RenderPass = uiRenderPass.get();
    VkPipelineRenderingCreateInfoKHR pipelineCreate{};
    pipelineCreate.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    pipelineCreate.colorAttachmentCount    = 1;
    pipelineCreate.pColorAttachmentFormats = swapchain.getImageFormat();

    initInfo.RenderPass                  = VK_NULL_HANDLE;
    initInfo.UseDynamicRendering         = true;
    initInfo.PipelineRenderingCreateInfo = pipelineCreate;

    initInfo.DescriptorPool = uiDescriptorPool.get();
    initInfo.MinImageCount  = 2;
    initInfo.ImageCount     = MAX_FRAME_IN_FLIGHT;
    initInfo.MSAASamples    = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&initInfo);
}

void VkSmol::recreatePerFrameSynchObjects() {
    for (size_t i = 0; i < MAX_FRAME_IN_FLIGHT; i++) {
        for (Semaphore& semaphore : renderFinishedSemaphores[i])
            semaphore.destroy(device);
        renderFinishedSemaphores[i].clear();

        for (Semaphore& semaphore : uiFinishedSemaphores[i])
            semaphore.destroy(device);
        uiFinishedSemaphores[i].clear();
    }

    renderFinishedSemaphores.clear();
    uiFinishedSemaphores.clear();

    renderFinishedSemaphores.resize(MAX_FRAME_IN_FLIGHT);
    uiFinishedSemaphores.resize(MAX_FRAME_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAME_IN_FLIGHT; i++) {
        renderFinishedSemaphores[i].resize(swapchain.getImageCount());
        for (Semaphore& semaphore : renderFinishedSemaphores[i])
            semaphore.init(device);

        uiFinishedSemaphores[i].resize(swapchain.getImageCount());
        for (Semaphore& semaphore : uiFinishedSemaphores[i])
            semaphore.init(device);
    }
}

void VkSmol::recreateSwapchain() {
    window.pauseWhileMinimized();

    swapchain.recreate(device, physicalDevice, surface, window);
    swapchainGeneration++;
    framebufferResized = false;
    
    recreatePerFrameSynchObjects();
}

std::vector<const char*> VkSmol::getRequiredExtensions() {
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

int VkSmol::scoreDevice(const PhysicalDevice& device) {
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

    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device.get(), &supportedFeatures);
    if (!supportedFeatures.samplerAnisotropy)
        return 0;

    if (!supportedFeatures.fragmentStoresAndAtomics)
        return 0;

    // Check swap chain support
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device, surface);
    if (swapChainSupport.formats.empty() || swapChainSupport.presentModes.empty())
        return 0;

    return 1;
}
