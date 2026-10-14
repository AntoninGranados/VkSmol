#include "imgui_layer.hpp"

#include <stdexcept>

#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_vulkan.h"

#include "../engine.hpp"
#include "../graph/render_graph.hpp"
#include "../graph/pass_context.hpp"
#include "../present/window.hpp"

namespace VkSmol {

ImGuiLayer::ImGuiLayer(Engine& engine, Window& window, std::optional<VkFormat> format): engine(engine) {
    Device& device = engine.getDevice();
    if (!format && engine.isHeadless())
        throw std::runtime_error("ImGuiLayer needs a color format when the engine has no swapchain");
    colorFormat = format.value_or(engine.getSwapchainFormat());

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_WindowBg]       = ImVec4(0.0f, 0.0f, 0.0f, 0.30f);
    style.Colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    ImGui_ImplGlfw_InitForVulkan(window.getWindow(), true);

    descriptorPool.addPoolSize(VK_DESCRIPTOR_TYPE_SAMPLER, 1000);
    descriptorPool.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000);
    descriptorPool.addPoolSize(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000);
    descriptorPool.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000);
    descriptorPool.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000);
    descriptorPool.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000);
    descriptorPool.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000);
    descriptorPool.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000);
    descriptorPool.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000);
    descriptorPool.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000);
    descriptorPool.addPoolSize(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 100);
    descriptorPool.init(device.getLogicalDevice(), 1000);

    VkPipelineRenderingCreateInfoKHR pipelineCreate{};
    pipelineCreate.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    pipelineCreate.colorAttachmentCount    = 1;
    pipelineCreate.pColorAttachmentFormats = &colorFormat;

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance                    = device.getInstance().get();
    initInfo.PhysicalDevice              = device.getPhysicalDevice().get();
    initInfo.Device                      = device.getLogicalDevice().get();
    initInfo.QueueFamily                 = device.getQueueFamilies().graphicsFamily.value();
    initInfo.Queue                       = device.getGraphicsQueue().get();
    initInfo.RenderPass                  = VK_NULL_HANDLE;
    initInfo.UseDynamicRendering         = true;
    initInfo.PipelineRenderingCreateInfo = pipelineCreate;
    initInfo.DescriptorPool              = descriptorPool.get();
    initInfo.MinImageCount               = 2;
    initInfo.ImageCount                  = MAX_FRAME_IN_FLIGHT;
    initInfo.MSAASamples                 = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&initInfo);
}

ImGuiLayer::~ImGuiLayer() {
    engine.waitIdle();
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    descriptorPool.destroy(engine.getDevice().getLogicalDevice());
}

void ImGuiLayer::newFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::render(PassContext& context) {
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), context.cmd());
}

ImTextureID ImGuiLayer::addTexture(VkSampler sampler, VkImageView view, VkImageLayout layout) {
    return reinterpret_cast<ImTextureID>(ImGui_ImplVulkan_AddTexture(sampler, view, layout));
}

ImTextureID ImGuiLayer::addTexture(const RenderGraph& graph, ImageHandle image) {
    return addTexture(graph.getSampler(image), graph.getView(image));
}

void ImGuiLayer::removeTexture(ImTextureID texture) {
    ImGui_ImplVulkan_RemoveTexture(reinterpret_cast<VkDescriptorSet>(texture));
}

}
