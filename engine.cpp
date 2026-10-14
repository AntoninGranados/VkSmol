#include "engine.hpp"

#include <cassert>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_vulkan.h"

namespace VkSmol {

Engine::Engine(const std::string& appName, uint32_t appVersion, std::optional<Window&> window)
    : window(window), device(appName, appVersion, window), logicalDevice(device.getLogicalDevice()) {
    if (!window) return;

    swapchain.emplace(device, window->getExtent());
    imageAvailableSemaphores.resize(MAX_FRAME_IN_FLIGHT);
    for (Semaphore& semaphore : imageAvailableSemaphores)
        semaphore.init(logicalDevice);
    window->setResizeCallback([this](int, int) { framebufferResized = true; });

    initImgui();
}

Engine::~Engine() {
    device.waitIdle();

    if (window) {
        ImGui_ImplVulkan_Shutdown();
        window->imguiGlfwShutdown();
        ImGui::DestroyContext();
        uiDescriptorPool.destroy(logicalDevice);
        for (Semaphore& semaphore : imageAvailableSemaphores)
            semaphore.destroy(logicalDevice);
        window->setResizeCallback({});
    }
}

std::optional<uint32_t> Engine::acquireImage() {
    assert(swapchain);
    const std::optional<uint32_t> imageIndex = swapchain->acquire(imageAvailableSemaphores[currentFrame]);
    if (!imageIndex) recreateSwapchain();
    return imageIndex;
}

void Engine::presentImage(uint32_t imageIndex, const Semaphore& waitSemaphore) {
    assert(swapchain);
    const bool outOfDate = swapchain->present(imageIndex, waitSemaphore);
    if (outOfDate || framebufferResized) recreateSwapchain();
    currentFrame = (currentFrame + 1) % MAX_FRAME_IN_FLIGHT;
}

void Engine::initImgui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_WindowBg]       = ImVec4(0.0f, 0.0f, 0.0f, 0.30f);
    style.Colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    window->imguiInitGlfwBackend();
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance       = device.getInstance().get();
    initInfo.PhysicalDevice = device.getPhysicalDevice().get();
    initInfo.Device         = logicalDevice.get();
    initInfo.QueueFamily    = device.getQueueFamilies().graphicsFamily.value();
    initInfo.Queue          = device.getGraphicsQueue().get();

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
    uiDescriptorPool.init(logicalDevice, 1000);

    VkPipelineRenderingCreateInfoKHR pipelineCreate{};
    pipelineCreate.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    pipelineCreate.colorAttachmentCount    = 1;
    uiColorFormat = swapchain->getImageFormat();
    pipelineCreate.pColorAttachmentFormats = &uiColorFormat;

    initInfo.RenderPass                  = VK_NULL_HANDLE;
    initInfo.UseDynamicRendering         = true;
    initInfo.PipelineRenderingCreateInfo = pipelineCreate;

    initInfo.DescriptorPool = uiDescriptorPool.get();
    initInfo.MinImageCount  = 2;
    initInfo.ImageCount     = MAX_FRAME_IN_FLIGHT;
    initInfo.MSAASamples    = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&initInfo);
}

void Engine::recreateSwapchain() {
    assert(swapchain);

    window->pauseWhileMinimized();
    swapchain->recreate(window->getExtent());
    framebufferResized = false;
}

}
