#include "application.hpp"

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const uint32_t MAX_FRAME_IN_FLIGHT = 2; // Number of frames that can be worked on in parallele by the CPU

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


const std::vector<Vertex> vertices = {
    { {-0.5f,-0.5f}, {0.0f, 0.0f, 0.0f} },
    { { 0.5f,-0.5f}, {1.0f, 0.0f, 0.0f} },
    { { 0.5f, 0.5f}, {1.0f, 1.0f, 0.0f} },
    { {-0.5f, 0.5f}, {0.0f, 1.0f, 0.0f} }
};

const std::vector<uint16_t> indices = {
    0, 1, 2, 2, 3, 0
};


void Application::run() {
    init();
    mainLoop();
    cleanup();
}

/*
============== INITIALISATION ==============
*/

void Application::init() {
    glfwInit();

    window.init(WIDTH, HEIGHT, "Application", framebufferResizeCallback);

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
    transferQueue.init(device, queueFamilyIndices.transferFamily.value());

    swapchain.init(device, physicalDevice, surface, window);

    // Render Pass
    {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = swapchain.getImageFormat();
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
        renderPass.addAttachment(colorAttachment, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        renderPass.addSubpass(VK_PIPELINE_BIND_POINT_GRAPHICS, { 0 });
        renderPass.addDependency(
            VK_SUBPASS_EXTERNAL, 0,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
        );
        renderPass.init(device);
    }
    
    // Graphics Pipeline
    {
        Shader vertShader, fragShader;
        vertShader.init(device, VK_SHADER_STAGE_VERTEX_BIT, "./shader/vert.glsl");
        fragShader.init(device, VK_SHADER_STAGE_FRAGMENT_BIT, "./shader/frag.glsl");
    
        VertexInput<Vertex> vertexInput;
        vertexInput.addAttributeDescription(VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, position));
        vertexInput.addAttributeDescription(VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color));

        uboLayout.addBinding(VK_SHADER_STAGE_VERTEX_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        uboLayout.init(device);
    
        graphicsPipeline.init(
            device, renderPass,
            vertexInput.get(),
            { vertShader, fragShader },
            { uboLayout }
        );
    
        // TODO: should be delayed in case we want to rebuild the pipeline and reuse the shaders
        vertShader.destroy(device);
        fragShader.destroy(device);
    }

    swapchain.initFramebuffer(device, renderPass);

    graphicsCommandPool.init(device, queueFamilyIndices.graphicsFamily.value());
    transferCommandPool.init(device, queueFamilyIndices.transferFamily.value());
    commandBuffers.resize(MAX_FRAME_IN_FLIGHT);
    for (CommandBuffer& commandBuffer : commandBuffers)
        commandBuffer.alloc(device, graphicsCommandPool);

    initImgui();
    
    // Buffers
    {
        // Vertex Buffer
        Buffer vertexStagingBuffer;
        size_t vertexBufferSize = sizeof(Vertex)*vertices.size();
        vertexStagingBuffer.init(device, vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        vertexStagingBuffer.alloc(device, physicalDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vertexStagingBuffer.fill(device, (void*)vertices.data());

        vertexBuffer.init(device, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        vertexBuffer.alloc(device, physicalDevice, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        Buffer::copy(vertexStagingBuffer, vertexBuffer, device, transferCommandPool, transferQueue);

        vertexStagingBuffer.free(device);
        vertexStagingBuffer.destroy(device);
        
        // Index Buffer
        Buffer indexStagingBuffer;
        size_t indexBufferSize = sizeof(indices[0])*indices.size();
        indexStagingBuffer.init(device, indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        indexStagingBuffer.alloc(device, physicalDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        indexStagingBuffer.fill(device, (void*)indices.data());
        
        indexBuffer.init(device, indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        indexBuffer.alloc(device, physicalDevice, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        Buffer::copy(indexStagingBuffer, indexBuffer, device, transferCommandPool, transferQueue);
        
        indexStagingBuffer.free(device);
        indexStagingBuffer.destroy(device);

        // Uniform Buffers
        size_t uniformBufferSize = sizeof(UBO);
        uniformBuffers.resize(MAX_FRAME_IN_FLIGHT);
        for (Buffer& uniformBuffer : uniformBuffers) {
            uniformBuffer.init(device, uniformBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, true);
            uniformBuffer.alloc(device, physicalDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        }
    }

    // Descriptor
    {
        descriptorPool.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAME_IN_FLIGHT);
        descriptorPool.init(device);
    
        descriptorSets.resize(MAX_FRAME_IN_FLIGHT);
        // DescriptorSet::alloc(descriptorSets, device, descriptorPool, std::vector<DescriptorSetLayout>(MAX_FRAME_IN_FLIGHT, uboLayout));
        for (size_t i = 0; i < MAX_FRAME_IN_FLIGHT; i++) {
            descriptorSets[i].alloc(device, descriptorPool, uboLayout); //! Should not allocate them individually
            descriptorSets[i].linkBuffer(device, uniformBuffers[i]);
        }
    }

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
}

void Application::initImgui() {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice, surface);

    uiCommandPool.init(device, indices.graphicsFamily.value());
    uiCommandBuffers.resize(MAX_FRAME_IN_FLIGHT);
    for (CommandBuffer& commandBuffer : uiCommandBuffers)
        commandBuffer.alloc(device, uiCommandPool);

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchain.getImageFormat();
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    uiRenderPass.addAttachment(colorAttachment, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    uiRenderPass.addSubpass(VK_PIPELINE_BIND_POINT_GRAPHICS, { 0 });
    uiRenderPass.addDependency(
        VK_SUBPASS_EXTERNAL, 0,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
    );
    uiRenderPass.init(device);
    
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();
    ImGui::GetStyle().Colors[ImGuiCol_WindowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.95f);
    ImGui::GetStyle().Colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    ImGui_ImplGlfw_InitForVulkan(window.get(), true);
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = instance.get();
    initInfo.PhysicalDevice = physicalDevice.get();
    initInfo.Device = device.get();
    initInfo.QueueFamily = indices.graphicsFamily.value();
    initInfo.Queue = graphicsQueue.get();

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

    initInfo.DescriptorPool = uiDescriptorPool.get();
    initInfo.RenderPass = uiRenderPass.get();
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = MAX_FRAME_IN_FLIGHT;
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&initInfo);
}


void Application::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    Application* pApp = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
    pApp->framebufferResized = true;
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


void Application::recordCommandBuffer(CommandBuffer commandBuffer, uint32_t imageIndex) {
    commandBuffer.beginRecording();
    {
        renderPass.begin(
            commandBuffer,
            swapchain,
            swapchain.getFramebuffer().getFrame(imageIndex),
            { }
        );
        graphicsPipeline.bind(commandBuffer);

        vertexBuffer.bindVertex(commandBuffer);
        indexBuffer.bindIndex(commandBuffer, VK_INDEX_TYPE_UINT16);
    
        // TODO: make a wrapper for the viewport
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)swapchain.getExtent().width;
        viewport.height = (float)swapchain.getExtent().height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        graphicsPipeline.setViewport(commandBuffer, viewport);
    
        VkRect2D scissor;
        scissor.offset = {0, 0};
        scissor.extent = swapchain.getExtent();
        graphicsPipeline.setScissor(commandBuffer, scissor);

        descriptorSets[currentFrame].bind(commandBuffer, graphicsPipeline.getLayout());
        graphicsPipeline.drawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()));

        renderPass.end(commandBuffer);
    }
    commandBuffer.endRecording();
}

void Application::recordUiCommandBuffer(CommandBuffer commandBuffer, uint32_t imageIndex) {
    commandBuffer.beginRecording();
    {
        uiRenderPass.begin(
            commandBuffer,
            swapchain,
            swapchain.getFramebuffer().getFrame(imageIndex),
            {{{0.0f, 0.0f, 0.0f, 0.0f}}}
        );
            
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        
        ImGui::NewFrame();

        ImGuiID dockspace_id = ImGui::GetID("Dock space");
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::DockSpaceOverViewport(dockspace_id, ImGui::GetMainViewport(), 0);

        ImGui::SetNextWindowPos({0, 0});
        ImGui::Begin("FPS",
            nullptr,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoDecoration // | ImGuiWindowFlags_NoBackground
        );
            ImGui::Text("%.1f fps (%.3f ms)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
        ImGui::End();

        ImGui::ShowDemoWindow();

        ImGui::Render();

        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer.get());

        uiRenderPass.end(commandBuffer);
    }
    commandBuffer.endRecording();
}

void Application::updateUniformBuffer(uint32_t currentFrame) {
    static auto startTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    UBO ubo{};
    ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.view = glm::lookAt(glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj = glm::perspective(glm::radians(60.0f), swapchain.getExtent().width / (float)swapchain.getExtent().height, 0.1f, 10.0f);
    ubo.proj[1][1] *= -1;   // Y coordinates are reversed compared to OpenGL

    uniformBuffers[currentFrame].fill(device, &ubo);
}


void Application::mainLoop() {
    while (!window.shouldClose()) {
        glfwPollEvents();
        drawFrame();
    }
}

void Application::drawFrame() {
    Fence::wait({ renderInFlightFences[currentFrame], uiInFlightFences[currentFrame] }, device);

    uint32_t result;
    result = swapchain.getNextImageIndex(device, imageAvailableSemaphores[currentFrame]);
    if (result == RECREATE_SWAPCHAIN) {
        swapchain.recreate(device, physicalDevice, surface, window, renderPass);
        return;
    }

    Fence::reset({ renderInFlightFences[currentFrame], uiInFlightFences[currentFrame] }, device);
    uint32_t imageIndex = result;

    updateUniformBuffer(currentFrame);

    commandBuffers[currentFrame].reset();
    recordCommandBuffer(commandBuffers[currentFrame], imageIndex);
    graphicsQueue.submit(
        { commandBuffers[currentFrame] },
        renderInFlightFences[currentFrame],
        { imageAvailableSemaphores[currentFrame] }, { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT },
        { renderFinishedSemaphores[currentFrame][imageIndex] }
    );

    uiCommandBuffers[currentFrame].reset();
    recordUiCommandBuffer(uiCommandBuffers[currentFrame], imageIndex);
    graphicsQueue.submit(
        { uiCommandBuffers[currentFrame] },
        uiInFlightFences[currentFrame],
        { renderFinishedSemaphores[currentFrame][imageIndex] }, { VK_PIPELINE_STAGE_VERTEX_INPUT_BIT },
        { uiFinishedSemaphores[currentFrame][imageIndex] }
    );

    result = presentQueue.present({ swapchain }, { imageIndex }, { uiFinishedSemaphores[currentFrame][imageIndex] });
    if (result == RECREATE_SWAPCHAIN || framebufferResized) {
        framebufferResized = false;
        swapchain.recreate(device, physicalDevice, surface, window, renderPass);
    }

    currentFrame = (currentFrame + 1) % MAX_FRAME_IN_FLIGHT;

    window.pauseWhileMinimized();
}


void Application::cleanup() {
    device.waitIdle();

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    for (size_t i = 0; i < MAX_FRAME_IN_FLIGHT; i++) {
        imageAvailableSemaphores[i].destroy(device);
        for (Semaphore& semaphore : renderFinishedSemaphores[i])
            semaphore.destroy(device);
        for (Semaphore& semaphore : uiFinishedSemaphores[i])
            semaphore.destroy(device);
        renderInFlightFences[i].destroy(device);
        uiInFlightFences[i].destroy(device);
    }

    descriptorPool.destroy(device);
    uiDescriptorPool.destroy(device);

    vertexBuffer.free(device);
    vertexBuffer.destroy(device);
    indexBuffer.free(device);
    indexBuffer.destroy(device);
    for (Buffer& uniformBuffer : uniformBuffers) {
        uniformBuffer.free(device);
        uniformBuffer.destroy(device);
    }

    for (CommandBuffer& commandBuffer : commandBuffers)
        commandBuffer.free(device, graphicsCommandPool);
    for (CommandBuffer& commandBuffer : uiCommandBuffers)
        commandBuffer.free(device, uiCommandPool);
    graphicsCommandPool.destroy(device);
    transferCommandPool.destroy(device);
    uiCommandPool.destroy(device);

    swapchain.destroy(device);
    
    graphicsPipeline.destroy(device);
    uboLayout.destroy(device);

    uiRenderPass.destroy(device);
    renderPass.destroy(device);

    device.destroy();
    surface.destroy(instance);

    debugMessenger.destroy(instance);
    instance.destroy();

    window.destroy();
    glfwTerminate();
}
