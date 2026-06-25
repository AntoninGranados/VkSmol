#include "engine.hpp"
#include "engine_version.hpp"

#include <algorithm>
#include <cassert>
#include <memory>
#include <vector>

#include "engine/device/queue.hpp"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_vulkan.h"

struct GLFWwindow; // forward-declare so we can cast getNativeWindowHandle() without pulling GLFW in

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
    // VK_KHR_SWAPCHAIN_EXTENSION_NAME, // added automatically when not in headless mode
    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
#ifdef __APPLE__
    "VK_KHR_portability_subset",
#endif
};

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation",
};

void VkSmol::init(std::string appName, uint32_t appVersion, Platform& p) {
    platform = &p;

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = appName.c_str();
    appInfo.applicationVersion = appVersion;
    appInfo.pEngineName = "VkSmol";
    appInfo.engineVersion = VK_SMOL_VERSION;
    appInfo.apiVersion = VK_API_VERSION_1_4;

    if (!platform->isHeadless()) {
        displayState = WindowedState{};
        platform->setResizeCallback([this](int, int) { framebufferResized = true; });
    }

    instance.init(appInfo, getRequiredInstanceExtensions(), validationLayers);
    debugMessenger.init(instance);

    if (!platform->isHeadless()) {
        VkSurfaceKHR rawSurface = platform->createSurface(instance.get());
        std::get<WindowedState>(displayState).surface.initFromRaw(rawSurface);
    }

    std::function<int(PhysicalDevice)> scoring = [this](PhysicalDevice device) { return scoreDevice(device); };
    physicalDevice.init(instance, scoring);

    QueueFamilyIndices queueFamilyIndices;
    if (!platform->isHeadless()) {
        queueFamilyIndices = findQueueFamilies(physicalDevice, std::get<WindowedState>(displayState).surface);
    } else {
        queueFamilyIndices = findQueueFamiliesHeadless(physicalDevice);
    }

    device.init(
        physicalDevice,
        queueFamilyIndices.getIndices(platform->isHeadless()),
        getRequiredDeviceExtensions(),
        validationLayers
    );

    graphicsQueue.init(device, queueFamilyIndices.graphicsFamily.value());
    transferQueue.init(device, queueFamilyIndices.transferFamily.value());

    if (!platform->isHeadless()) {
        std::get<WindowedState>(displayState).presentQueue.init(device, queueFamilyIndices.presentFamily.value());
        auto* glfwWindow = static_cast<GLFWwindow*>(platform->getNativeWindowHandle());
        std::get<WindowedState>(displayState).swapchain.init(
            device, physicalDevice,
            std::get<WindowedState>(displayState).surface,
            glfwWindow);
    } else {
        displayState = HeadlessState{};
        std::get<HeadlessState>(displayState).extent = platform->getExtent();
    }

    dynamicRenderer.init(device);
    graphicsCommandPool.init(device, queueFamilyIndices.graphicsFamily.value());
    transferCommandPool.init(device, queueFamilyIndices.transferFamily.value(), VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);

    if (!platform->isHeadless()) initImgui();
}


CommandBuffer VkSmol::allocCommandBuffer() {
    CommandBuffer cb;
    cb.alloc(device, graphicsCommandPool);
    return cb;
}

void VkSmol::freeCommandBuffer(CommandBuffer& cb) {
    cb.free(device, graphicsCommandPool);
}

void VkSmol::submit(
    CommandBuffer& commandBuffer,
    Fence& fence,
    const std::vector<const Semaphore*>& waitSemaphores,
    const std::vector<VkPipelineStageFlags>& waitStages,
    const std::vector<const Semaphore*>& signalSemaphores
) {
    commandBuffer.endRecording();
    graphicsQueue.submit({ &commandBuffer }, fence, waitSemaphores, waitStages, signalSemaphores);
}

void VkSmol::flushAll() {
    for (auto& queue : retirementQueue) {
        for (auto& payload : queue) payload(device);
        queue.clear();
    }
}

void VkSmol::waitIdle() {
    device.waitIdle();
    flushAll();
}

void VkSmol::terminate() {
    device.waitIdle();

    if (!platform->isHeadless()) {
        ImGui_ImplVulkan_Shutdown();
        platform->imguiGlfwShutdown();
        ImGui::DestroyContext();
    }

    flushAll();

    assert(ownedImages.empty() && ownedViews.empty() && ownedSamplers.empty());
    assert(ownedBuffers.empty() && ownedPerFrameBuffers.empty());
    assert(passDescriptorLayouts.empty() && passDescriptorGroups.empty());
    assert(ownedComputePipelines.empty() && ownedGraphicsPipelines.empty());

    if (!platform->isHeadless()) uiDescriptorPool.destroy(device);

    graphicsCommandPool.destroy(device);
    transferCommandPool.destroy(device);

    if (!platform->isHeadless()) std::get<WindowedState>(displayState).swapchain.destroy(device);

    device.destroy();
    if (!platform->isHeadless()) std::get<WindowedState>(displayState).surface.destroy(instance);

    debugMessenger.destroy(instance);
    instance.destroy();
}

GraphicsPipeline VkSmol::initGraphicsPipeline(
    VkPipelineVertexInputStateCreateInfo vertexInputInfo,
    const std::vector<const Shader*>& shaders,
    const std::vector<const DescriptorSetLayout*>& setLayouts,
    GraphicsPipeline other,
    VkFormat colorFormat
) {
    // Dynamic Rendering Info
    VkFormat format;
    if (colorFormat == VK_FORMAT_UNDEFINED) {
        format = platform->isHeadless() ? VK_FORMAT_R8G8B8A8_UNORM : *std::get<WindowedState>(displayState).swapchain.getImageFormat();
    } else {
        format = colorFormat;
    }
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

ComputePipeline VkSmol::initComputePipeline(
    const Shader& shader,
    const std::vector<const DescriptorSetLayout*>& setLayouts,
    ComputePipeline other
) {
    ComputePipeline pipeline;
    pipeline.init(device, shader, setLayouts, std::move(other));
    return pipeline;
}

void VkSmol::destroyComputePipeline(ComputePipeline& pipeline) {
    pipeline.destroy(device);
}

void VkSmol::reloadComputePipeline(
    ComputePipeline& pipeline,
    const std::string& shaderPath,
    const std::vector<const DescriptorSetLayout*>& setLayouts
) {
    Shader shader = createShader(VK_SHADER_STAGE_COMPUTE_BIT, shaderPath);
    ComputePipeline next = initComputePipeline(shader, setLayouts, pipeline);
    destroyComputePipeline(pipeline);
    pipeline = std::move(next);
    destroyShader(shader);
}

void VkSmol::reloadGraphicsPipeline(
    GraphicsPipeline& pipeline,
    VkPipelineVertexInputStateCreateInfo vertexInputInfo,
    const std::vector<std::pair<VkShaderStageFlagBits, std::string>>& shaderPaths,
    const std::vector<const DescriptorSetLayout*>& setLayouts
) {
    std::vector<Shader> shaders;
    shaders.reserve(shaderPaths.size());
    try {
        for (auto& [stage, path] : shaderPaths)
            shaders.push_back(createShader(stage, path));
    } catch (...) {
        for (auto& s : shaders) destroyShader(s);
        throw;
    }

    std::vector<const Shader*> ptrs;
    ptrs.reserve(shaders.size());
    for (auto& s : shaders) ptrs.push_back(&s);

    GraphicsPipeline next = initGraphicsPipeline(vertexInputInfo, ptrs, setLayouts, pipeline);
    destroyGraphicsPipeline(pipeline);
    pipeline = std::move(next);

    for (auto& s : shaders) destroyShader(s);
}

Shader VkSmol::createShader(VkShaderStageFlagBits stage, std::string filepath) {
    Shader shader;
    shader.init(device, stage, filepath);
    return shader;
}

void VkSmol::destroyShader(Shader& shader) {
    shader.destroy(device);
}

// ——— Descriptor sets ————————————————————————————————————————————————————————

void VkSmol::initDescriptorSetLayout(DescriptorSetLayout& layout) {
    layout.init(device);
}

void VkSmol::destroyDescriptorSetLayout(DescriptorSetLayout& layout) {
    layout.destroy(device);
}

DescriptorSetGroup VkSmol::createDescriptorSetGroup(DescriptorSetLayout& layout, std::string name) {
    const std::string baseName = name.empty() ? "DescriptorSet" : std::move(name);

    DescriptorPool pool;
    for (const auto& b : layout.getBindings())
        pool.addPoolSize(b.descriptorType, MAX_FRAME_IN_FLIGHT);
    pool.init(device, MAX_FRAME_IN_FLIGHT);

    std::vector<DescriptorSet> sets(MAX_FRAME_IN_FLIGHT);
    for (size_t i = 0; i < MAX_FRAME_IN_FLIGHT; ++i) {
        sets[i].alloc(device, pool, layout);
        sets[i].setResourceName(baseName + '[' + std::to_string(i) + ']');
    }
    return DescriptorSetGroup(std::move(pool), std::move(sets));
}

void VkSmol::updateDescriptorSetGroup(DescriptorSetGroup& group, uint32_t frameIndex, const DescriptorWriter& writer) {
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
    set.update(device);
}

void VkSmol::destroyDescriptorSetGroup(DescriptorSetGroup& group) {
    if (!group.isValid()) return;
    auto retired = std::make_shared<DescriptorSetGroup>(std::move(group));
    retirementQueue[currentRetirementFrame].push_back(
        [retired](const LogicalDevice& d) { retired->destroy(d); }
    );
    group = {};
}

// ——— Buffers ————————————————————————————————————————————————————————————————

Buffer VkSmol::createBuffer(VkBufferUsageFlags usage, size_t size, void* data, std::string name) {
    Buffer buf;
    buf.setResourceName(name.empty() ? "Buffer" : std::move(name));
    if (data == nullptr) {
        buf.init(device, usage, size);
    } else {
        buf.initFilled(device, physicalDevice, usage, transferCommandPool, transferQueue, size, data);
    }
    return buf;
}

Buffer VkSmol::createMappedBuffer(VkBufferUsageFlags usage, size_t size, std::string name) {
    Buffer buf;
    buf.setResourceName(name.empty() ? "MappedBuffer" : std::move(name));
    buf.init(device, usage, size, true);
    buf.alloc(device, physicalDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    return buf;
}

Buffer VkSmol::createReadbackBuffer(size_t size, bool persistentMapping, std::string name) {
    Buffer buf;
    buf.setResourceName(name.empty() ? "ReadbackBuffer" : std::move(name));
    buf.init(device, VK_BUFFER_USAGE_TRANSFER_DST_BIT, size, persistentMapping);
    buf.alloc(device, physicalDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    return buf;
}

void VkSmol::fillBuffer(Buffer& buffer, void* data) {
    buffer.fill(device, data);
}

void VkSmol::readBuffer(Buffer& buffer, void* out, size_t bytes, size_t offset) {
    buffer.read(device, out, bytes, offset);
}

void VkSmol::destroyBuffer(Buffer& buffer) {
    if (!buffer.get()) return;
    auto retired = std::make_shared<Buffer>(std::move(buffer));
    retirementQueue[currentRetirementFrame].push_back(
        [retired](const LogicalDevice& d) { retired->free(d); retired->destroy(d); }
    );
    buffer = Buffer{};
}

// ——— Images —————————————————————————————————————————————————————————————————

Image VkSmol::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, std::string name) {
    Image img;
    img.setResourceName(name.empty() ? "Image" : std::move(name));
    img.init(device, width, height, format, usage);
    img.alloc(device, physicalDevice, properties);
    return img;
}

Image VkSmol::createImageFromPath(std::string path, std::string name) {
    Image img;
    img.setResourceName(name.empty() ? "Image" : std::move(name));
    img.initFromPath(device, physicalDevice, transferCommandPool, transferQueue, path);
    return img;
}

void VkSmol::destroyImage(Image& image) {
    if (!image.get()) return;
    auto retired = std::make_shared<Image>(std::move(image));
    retirementQueue[currentRetirementFrame].push_back(
        [retired](const LogicalDevice& d) { retired->free(d); retired->destroy(d); }
    );
    image = Image{};
}

ImageView VkSmol::createImageView(Image& image, std::string name) {
    if (name.empty() && !image.getResourceName().empty())
        name = image.getResourceName() + "View";
    ImageView view;
    view.setResourceName(name.empty() ? "ImageView" : std::move(name));
    view.init(device, image);
    return view;
}

void VkSmol::destroyImageView(ImageView& view) {
    if (!view.get()) return;
    auto retired = std::make_shared<ImageView>(std::move(view));
    retirementQueue[currentRetirementFrame].push_back(
        [retired](const LogicalDevice& d) { retired->destroy(d); }
    );
    view = ImageView{};
}

Sampler VkSmol::createSampler(std::string name) {
    Sampler smp;
    smp.setResourceName(name.empty() ? "Sampler" : std::move(name));
    smp.init(device, physicalDevice);
    return smp;
}

void VkSmol::destroySampler(Sampler& sampler) {
    if (!sampler.get()) return;
    auto retired = std::make_shared<Sampler>(std::move(sampler));
    retirementQueue[currentRetirementFrame].push_back(
        [retired](const LogicalDevice& d) { retired->destroy(d); }
    );
    sampler = Sampler{};
}

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
        clearColor, (VkRect2D){ (VkOffset2D){}, getExtent() }
    );
}

void VkSmol::endDynamicRenderer(const CommandBuffer& commandBuffer) {
    dynamicRenderer.end(commandBuffer);
}


bool VkSmol::shouldTerminate() {
    return platform->shouldClose();
}

Image& VkSmol::getSwapchainImage(size_t imageIndex) {
    assert(!platform->isHeadless());
    return std::get<WindowedState>(displayState).swapchain.getImage(imageIndex);
}

ImageView& VkSmol::getSwapchainImageView(size_t imageIndex) {
    assert(!platform->isHeadless());
    return std::get<WindowedState>(displayState).swapchain.getImageView(imageIndex);
}

void VkSmol::initImgui() {
    assert(!platform->isHeadless());

    QueueFamilyIndices indices = findQueueFamilies(physicalDevice, std::get<WindowedState>(displayState).surface);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_WindowBg]       = ImVec4(0.0f, 0.0f, 0.0f, 0.30f);
    style.Colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    platform->imguiInitGlfwBackend();
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
    uiDescriptorPool.init(device, 1000);

    VkPipelineRenderingCreateInfoKHR pipelineCreate{};
    pipelineCreate.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    pipelineCreate.colorAttachmentCount    = 1;
    pipelineCreate.pColorAttachmentFormats = std::get<WindowedState>(displayState).swapchain.getImageFormat();

    initInfo.RenderPass                  = VK_NULL_HANDLE;
    initInfo.UseDynamicRendering         = true;
    initInfo.PipelineRenderingCreateInfo = pipelineCreate;

    initInfo.DescriptorPool = uiDescriptorPool.get();
    initInfo.MinImageCount  = 2;
    initInfo.ImageCount     = MAX_FRAME_IN_FLIGHT;
    initInfo.MSAASamples    = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&initInfo);
}

void VkSmol::recreateSwapchain() {
    assert(!platform->isHeadless());

    platform->pauseWhileMinimized();

    auto* glfwWindow = static_cast<GLFWwindow*>(platform->getNativeWindowHandle());
    Swapchain newSwapchain;
    newSwapchain.init(
        device, physicalDevice,
        std::get<WindowedState>(displayState).surface,
        glfwWindow,
        &std::get<WindowedState>(displayState).swapchain);

    auto retired = std::make_shared<Swapchain>(std::move(std::get<WindowedState>(displayState).swapchain));
    retirementQueue[currentRetirementFrame].push_back(
        [retired](const LogicalDevice& d) { retired->destroy(d); }
    );

    std::get<WindowedState>(displayState).swapchain = std::move(newSwapchain);
    std::get<WindowedState>(displayState).swapchainGeneration++;
    framebufferResized = false;
}

std::vector<const char*> VkSmol::getRequiredInstanceExtensions() {
    std::vector<const char*> requiredExtensions;

    for (const char* ext : platform->getRequiredInstanceExtensions())
        requiredExtensions.emplace_back(ext);

    requiredExtensions.insert(requiredExtensions.end(), instanceExtensions.begin(), instanceExtensions.end());

    return requiredExtensions;
}

std::vector<const char*> VkSmol::getRequiredDeviceExtensions() {
    std::vector<const char*> requiredExtensions;

    for (const char* ext : platform->getRequiredDeviceExtensions())
        requiredExtensions.emplace_back(ext);

    requiredExtensions.insert(requiredExtensions.end(), deviceExtensions.begin(), deviceExtensions.end());

    return requiredExtensions;
}

int VkSmol::scoreDevice(const PhysicalDevice& device) {
    QueueFamilyIndices indices = platform->isHeadless() ?
        findQueueFamiliesHeadless(device) :
        findQueueFamilies(device, std::get<WindowedState>(displayState).surface);
    if (!indices.isComplete(platform->isHeadless())) return 0;

    // Check if all extensions are supported by the GPU
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device.get(), nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> supportedExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device.get(), nullptr, &extensionCount, supportedExtensions.data());

    for (const char* extension : getRequiredDeviceExtensions()) {
        if (!std::any_of(
            supportedExtensions.begin(),
            supportedExtensions.end(),
            [&](VkExtensionProperties& properties) { return strcmp(extension, properties.extensionName) == 0; })
        )
            return 0;
    }

    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device.get(), &supportedFeatures);
    if (!supportedFeatures.samplerAnisotropy) return 0;

    if (!supportedFeatures.fragmentStoresAndAtomics) return 0;

    // Check swap chain support
    if (!platform->isHeadless()) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device, std::get<WindowedState>(displayState).surface);
        if (swapChainSupport.formats.empty() || swapChainSupport.presentModes.empty()) return 0;
    }

    return 1;
}
