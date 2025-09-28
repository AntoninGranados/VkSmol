#include "application.hpp"

const std::vector<Vertex> vertices = {
    { {-0.5f,-0.5f}, {0.0f, 0.0f, 0.0f} },
    { { 0.5f,-0.5f}, {1.0f, 0.0f, 0.0f} },
    { { 0.5f, 0.5f}, {1.0f, 1.0f, 0.0f} },
    { {-0.5f, 0.5f}, {0.0f, 1.0f, 0.0f} }
};

const std::vector<index_t> indices = {
    0, 1, 2, 2, 3, 0
};

Application::Application() {
    engine.init("Vulkan Application", VK_MAKE_API_VERSION(0, 1, 0, 0));

    Image image = engine.initImageFromPath("./res/logo.png");
    engine.destroyImage(image);

    // Buffer creation
    {
        vertexBuffer = engine.initBuffer(
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            sizeof(Vertex) * vertices.size(), (void*)vertices.data()
        );
        
        indexBuffer = engine.initBuffer(
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            sizeof(index_t) * indices.size(), (void*)indices.data()
        );
    
        uniformBuffers = engine.initBufferList(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(UBO));
    }
    
    setLayout.addBinding(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    // setLayout.addBinding(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    engine.initDescriptorSetLayout(setLayout);
    
    // Pipeline creation
    {
        Shader vertShader = engine.initShader(VK_SHADER_STAGE_VERTEX_BIT,   "./res/shader/vert.glsl");
        Shader fragShader = engine.initShader(VK_SHADER_STAGE_FRAGMENT_BIT, "./res/shader/frag.glsl");
    
        VertexInput<Vertex> vertexInput;
        vertexInput.addAttributeDescription(VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, position));
        vertexInput.addAttributeDescription(VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color));
        pipeline = engine.initGraphicsPipeline(
            vertexInput.get(),
            { vertShader, fragShader },
            { setLayout }
        );
    
        engine.destroyShader(vertShader);
        engine.destroyShader(fragShader);
    }

    descriptorSets = engine.initDescriptorSetList(setLayout, uniformBuffers);
    
    engine.destroyDescriptorSetLayout(setLayout);
}

Application::~Application() {
    engine.waitIdle();

    engine.destroyGraphicsPipeline(pipeline);
    
    engine.destroyBuffer(vertexBuffer);
    engine.destroyBuffer(indexBuffer);
    engine.destroyBufferList(uniformBuffers);
    
    engine.terminate();
}

void Application::run() {
    while(!engine.shouldTerminate()) {
        CommandBuffer commandBuffer;
    
        engine.beginFrame();
        
        commandBuffer = engine.beginRecordingRender();
        {
            VkExtent2D extent = engine.getExtent();
            
            static auto startTime = std::chrono::high_resolution_clock::now();
            auto currentTime = std::chrono::high_resolution_clock::now();
            float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
            
            UBO ubo{};
            ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
            ubo.view = glm::lookAt(glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
            ubo.proj = glm::perspective(glm::radians(60.0f), extent.width / (float)extent.height, 0.1f, 10.0f);
            ubo.proj[1][1] *= -1;
            
            engine.fillBuffer(engine.getBuffer(uniformBuffers), (void*)&ubo);
            engine.getDescriptorSet(descriptorSets).bind(commandBuffer, pipeline.getLayout());
            
            pipeline.bind(commandBuffer);
            
            vertexBuffer.bindVertex(commandBuffer);
            indexBuffer.bindIndex(commandBuffer, VK_INDEX_TYPE_UINT16);
            
            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = (float)extent.width;
            viewport.height = (float)extent.height;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            pipeline.setViewport(commandBuffer, viewport);
            
            VkRect2D scissor;
            scissor.offset = {0, 0};
            scissor.extent = extent;
            pipeline.setScissor(commandBuffer, scissor);
            
            pipeline.drawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()));
        }
        engine.endRecoringRender(commandBuffer);
            

        commandBuffer = engine.beginRecordingUiRender();
        {
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
            
            ImGui::Render();
            
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer.get());
        }
        engine.endRecoringUiRender(commandBuffer);

        engine.endFrame();
    }
}
