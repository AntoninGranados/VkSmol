#include "engine.hpp"

#include <algorithm>
#include <cassert>
#include <format>
#include <memory>
#include <stdexcept>
#include <vector>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_vulkan.h"

namespace VkSmol {

Engine::Engine(const std::string& appName, uint32_t appVersion, std::optional<Window&> window)
    : window(window), device(appName, appVersion, window), logicalDevice(device.getLogicalDevice()) {
    dynamicRenderer.init(logicalDevice);

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
    if (graphInitialized) destroyGraph();

    if (window) {
        ImGui_ImplVulkan_Shutdown();
        window->imguiGlfwShutdown();
        ImGui::DestroyContext();
    }

    for (StagingArena& arena : stagingArenas) device.destroyBuffer(arena.buffer);
    device.waitIdle();

    assert(ownedImages.empty() && ownedViews.empty() && ownedSamplers.empty());
    assert(ownedBuffers.empty() && pendingBufferTransfers.empty());
    assert(passDescriptorLayouts.empty() && passDescriptorGroups.empty());
    assert(ownedComputePipelines.empty() && ownedGraphicsPipelines.empty());

    if (window) {
        uiDescriptorPool.destroy(logicalDevice);
        for (Semaphore& semaphore : imageAvailableSemaphores)
            semaphore.destroy(logicalDevice);
        window->setResizeCallback({});
    }
}

void Engine::submit(
    CommandBuffer& commandBuffer,
    Fence& fence,
    const std::vector<const Semaphore*>& waitSemaphores,
    const std::vector<VkPipelineStageFlags2>& waitStages,
    const std::vector<const Semaphore*>& signalSemaphores
) {
    commandBuffer.endRecording();
    device.getGraphicsQueue().submit({ &commandBuffer }, fence, waitSemaphores, waitStages, signalSemaphores);
}

GraphicsPipeline Engine::initGraphicsPipeline(
    VkPipelineVertexInputStateCreateInfo vertexInputInfo,
    const std::vector<const Shader*>& shaders,
    const std::vector<const DescriptorSetLayout*>& setLayouts,
    GraphicsPipeline other,
    VkFormat colorFormat
) {
    // Dynamic Rendering Info
    VkFormat format;
    if (colorFormat == VK_FORMAT_UNDEFINED) {
        format = swapchain ? swapchain->getImageFormat() : VK_FORMAT_R8G8B8A8_UNORM;
    } else {
        format = colorFormat;
    }
    VkPipelineRenderingCreateInfoKHR pipelineCreate{};
    pipelineCreate.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    pipelineCreate.colorAttachmentCount    = 1;
    pipelineCreate.pColorAttachmentFormats = &format;

    GraphicsPipeline pipeline;
    pipeline.init(
        logicalDevice,
        pipelineCreate,
        vertexInputInfo,
        shaders,
        setLayouts,
        pipelineCache,
        other
    );
    return pipeline;
}

void Engine::destroyGraphicsPipeline(GraphicsPipeline& pipeline) {
    pipeline.destroy(logicalDevice);
}

ComputePipeline Engine::initComputePipeline(
    const Shader& shader,
    const std::vector<const DescriptorSetLayout*>& setLayouts,
    ComputePipeline other
) {
    ComputePipeline pipeline;
    pipeline.init(logicalDevice, shader, setLayouts, pipelineCache, std::move(other));
    return pipeline;
}

void Engine::destroyComputePipeline(ComputePipeline& pipeline) {
    pipeline.destroy(logicalDevice);
}

const std::vector<uint32_t>& Engine::compilePassShader(const CompiledPass& pass, VkShaderStageFlagBits stage, const std::string& path, const ShaderDefines& defines, std::vector<std::string>& errors) {
    const Device::CompiledShader& compiled = device.compileShader(path, defines);
    if (!compiled.error.empty())
        errors.push_back(compiled.error);

    if (compiled.compilation.stage != stage) {
        throw std::runtime_error(std::format("The shader [{}{}{}] does not have the stage expected by the pass [{}]", MSG_RED_B, path, MSG_RESET, pass.name));
    }

    for (const ShaderParameter& parameter : compiled.compilation.parameters) {
        if (!parameter.used) continue;

        const bool bound = std::ranges::any_of(pass.descriptorBindings, [&](const DescriptorBindingDecl& decl) {
            return decl.set == parameter.location.set && decl.binding == parameter.location.binding;
        });
        if (!bound) {
            throw std::runtime_error(std::format(
                "The pass [{}] does not bind the parameter [{}{}{}] (set {}, binding {}) used by the shader [{}]",
                pass.name, MSG_RED_B, parameter.name, MSG_RESET, parameter.location.set, parameter.location.binding, path
            ));
        }
    }
    return compiled.compilation.spirv;
}

// ——— Descriptor sets ————————————————————————————————————————————————————————

void Engine::initDescriptorSetLayout(DescriptorSetLayout& layout) {
    layout.init(logicalDevice);
}

void Engine::destroyDescriptorSetLayout(DescriptorSetLayout& layout) {
    layout.destroy(logicalDevice);
}

DescriptorSetGroup Engine::createDescriptorSetGroup(DescriptorSetLayout& layout, std::string name) {
    const std::string baseName = name.empty() ? "DescriptorSet" : std::move(name);

    DescriptorPool pool;
    for (const auto& b : layout.getBindings())
        pool.addPoolSize(b.descriptorType, MAX_FRAME_IN_FLIGHT);
    pool.init(logicalDevice, MAX_FRAME_IN_FLIGHT);

    std::vector<DescriptorSet> sets(MAX_FRAME_IN_FLIGHT);
    for (size_t i = 0; i < MAX_FRAME_IN_FLIGHT; ++i) {
        sets[i].alloc(logicalDevice, pool, layout);
        sets[i].setResourceName(baseName + '[' + std::to_string(i) + ']');
    }
    return DescriptorSetGroup(std::move(pool), std::move(sets));
}

void Engine::updateDescriptorSetGroup(DescriptorSetGroup& group, uint32_t frameIndex, const DescriptorWriter& writer) {
    DescriptorSet& set = group.at(frameIndex);
    set.clear();
    for (const DescriptorWrite& write : writer.getWrites()) {
        std::visit([&](const auto& payload) {
            using T = std::decay_t<decltype(payload)>;
            if constexpr (std::is_same_v<T, UniformBufferPayload>) {
                assert(payload.buffer);
                set.linkUniformBuffer(*payload.buffer, write.binding);
            } else if constexpr (std::is_same_v<T, StorageBufferPayload>) {
                assert(payload.buffer);
                set.linkStorageBuffer(*payload.buffer, write.binding);
            } else if constexpr (std::is_same_v<T, CombinedImageSamplerPayload>) {
                assert(payload.view && payload.sampler);
                set.linkCombinedImageSampler(*payload.view, *payload.sampler, write.binding);
            } else if constexpr (std::is_same_v<T, StorageImagePayload>) {
                assert(payload.view);
                set.linkStorageImage(*payload.view, write.binding);
            }
        }, write.payload);
    }
    set.update(logicalDevice);
}

void Engine::destroyDescriptorSetGroup(DescriptorSetGroup& group) {
    if (!group.isValid()) return;
    auto retired = std::make_shared<DescriptorSetGroup>(std::move(group));
    device.retire([retired](const LogicalDevice& d) { retired->destroy(d); });
    group = {};
}

void Engine::beginDynamicRenderer(
    const CommandBuffer& commandBuffer,
    VkImageView imageView, VkImageLayout imageLayout,
    VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp,
    VkExtent2D extent,
    VkClearColorValue clearColor
) {
    dynamicRenderer.begin(
        commandBuffer,
        imageView, imageLayout,
        loadOp, storeOp,
        clearColor, (VkRect2D){ (VkOffset2D){}, extent }
    );
}

void Engine::endDynamicRenderer(const CommandBuffer& commandBuffer) {
    dynamicRenderer.end(commandBuffer);
}

Image& Engine::getSwapchainImage(size_t imageIndex) {
    assert(swapchain);
    return swapchain->getImage(imageIndex);
}

ImageView& Engine::getSwapchainImageView(size_t imageIndex) {
    assert(swapchain);
    return swapchain->getImageView(imageIndex);
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
    swapchainGeneration++;
    framebufferResized = false;
}

}
