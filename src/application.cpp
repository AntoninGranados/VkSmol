#include "application.hpp"

const std::vector<Vertex> vertices = {
    { .position={ 1.0f, 1.0f}, .color={1.0f, 1.0f, 0.0f}, .texCoord={0.0f, 1.0f} },
    { .position={ 1.0f,-1.0f}, .color={1.0f, 0.0f, 0.0f}, .texCoord={0.0f, 0.0f} },
    { .position={-1.0f,-1.0f}, .color={0.0f, 0.0f, 0.0f}, .texCoord={1.0f, 0.0f} },
    { .position={-1.0f, 1.0f}, .color={0.0f, 1.0f, 0.0f}, .texCoord={1.0f, 1.0f} }
};

const std::vector<ScreenVertex> screenVertices = {
    { .position={ 1.0f, 1.0f} },
    { .position={ 1.0f,-1.0f} },
    { .position={-1.0f,-1.0f} },
    { .position={-1.0f, 1.0f} }
};

const std::vector<index_t> indices = {
    0, 1, 2, 2, 3, 0
};

Application::Application() {
    engine.init("Vulkan Application", VK_MAKE_API_VERSION(0, 1, 0, 0));

    // Buffer creation
    {
        vertexBuffer = engine.initBuffer(
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            sizeof(Vertex) * vertices.size(), (void*)vertices.data()
        );
        
        screenVertexBuffer = engine.initBuffer(
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            sizeof(ScreenVertex) * screenVertices.size(), (void*)screenVertices.data()
        );
        
        indexBuffer = engine.initBuffer(
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            sizeof(index_t) * indices.size(), (void*)indices.data()
        );
    
        uniformBuffers = engine.initBufferList(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(UBO));
    }

    // Image (image + view + sampler) creation
    {
        VkExtent2D extent = engine.getExtent();
        for (size_t i = 0; i < 2; i++) {
            images[i] = engine.initImage(
                extent.width, extent.height,
                VK_FORMAT_B8G8R8A8_SRGB,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
            );
            imageViews[i] = engine.initImageView(images[i]);
            samplers[i] = engine.initSampler();
        }
    }
    
    setLayout.addBinding(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    setLayout.addBinding(VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    engine.initDescriptorSetLayout(setLayout);
    
    screenSetLayout.addBinding(VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    engine.initDescriptorSetLayout(screenSetLayout);
    
    // Pipeline creation
    {
        Shader vertShader = engine.initShader(VK_SHADER_STAGE_VERTEX_BIT,   "./res/shader/vert.glsl");
        Shader fragShader = engine.initShader(VK_SHADER_STAGE_FRAGMENT_BIT, "./res/shader/frag.glsl");
    
        VertexInput<Vertex> vertexInput;
        vertexInput.addAttributeDescription(VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, position));
        vertexInput.addAttributeDescription(VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color));
        vertexInput.addAttributeDescription(VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, texCoord));
        pipeline = engine.initGraphicsPipeline(
            vertexInput.get(),
            { vertShader, fragShader },
            { setLayout }
        );
    
        engine.destroyShader(vertShader);
        engine.destroyShader(fragShader);
        
        Shader screenVertShader = engine.initShader(VK_SHADER_STAGE_VERTEX_BIT,   "./res/shader/screenVert.glsl");
        Shader screenFragShader = engine.initShader(VK_SHADER_STAGE_FRAGMENT_BIT, "./res/shader/screenFrag.glsl");
        
        VertexInput<ScreenVertex> screenVertexInput;
        screenVertexInput.addAttributeDescription(VK_FORMAT_R32G32_SFLOAT, offsetof(ScreenVertex, position));
        screenPipeline = engine.initGraphicsPipeline(
            screenVertexInput.get(),
            { screenVertShader, screenFragShader },
            { screenSetLayout }
        );
        
        engine.destroyShader(screenVertShader);
        engine.destroyShader(screenFragShader);
    }

    std::vector<std::pair<ImageView, Sampler> > combinedImageSampler = {
        { imageViews[0], samplers[0] },
        { imageViews[1], samplers[1] }
    };
    
    for (size_t i = 0; i < 2; i++) {
        descriptorSets[i] = engine.initDescriptorSetList(
            setLayout,
            { &uniformBuffers, &combinedImageSampler[1-i] }
        );
        screenDescriptorSets[i] = engine.initDescriptorSetList(
            screenSetLayout,
            { &combinedImageSampler[i] }
        );
    }
    
    engine.destroyDescriptorSetLayout(setLayout);
    engine.destroyDescriptorSetLayout(screenSetLayout);
}

Application::~Application() {
    engine.waitIdle();

    for (size_t i = 0; i < 2; i++) {
        engine.destroySampler(samplers[i]);
        engine.destroyImage(images[i]);
        engine.destroyImageView(imageViews[i]);
    }

    engine.destroyBuffer(vertexBuffer);
    engine.destroyBuffer(screenVertexBuffer);
    engine.destroyBuffer(indexBuffer);
    engine.destroyBufferList(uniformBuffers);
    
    engine.destroyGraphicsPipeline(pipeline);
    engine.destroyGraphicsPipeline(screenPipeline);
    
    engine.terminate();
}

void Application::run() {
    auto startTime = std::chrono::high_resolution_clock::now();

    while(!engine.shouldTerminate()) {
        frame = (frame + 1) % 2;
        frameCount++;
        CommandBuffer commandBuffer;
    
        engine.beginFrame();
        
        commandBuffer = engine.beginRecordingRender();
        {
            VkExtent2D extent = engine.getExtent();

            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = (float)extent.width;
            viewport.height = (float)extent.height;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            
            VkRect2D scissor;
            scissor.offset = {0, 0};
            scissor.extent = extent;
            
            {   // Rendering
                engine.barrier(
                    commandBuffer,
                    images[1-frame].get(),
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_ACCESS_NONE, VK_ACCESS_SHADER_READ_BIT,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                );
                engine.barrier(
                    commandBuffer,
                    images[frame].get(),
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_ACCESS_NONE, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                );
                engine.beginDynamicRenderer(
                    commandBuffer,
                    imageViews[frame].get(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
                    {{ 0.0f, 0.0f, 0.0f, 1.0f }}
                );

                auto currentTime = std::chrono::high_resolution_clock::now();
                float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
                startTime = currentTime;
                rotation += deltaTime * rotationSpeed;

                UBO ubo{};
                ubo.time = glfwGetTime();
                ubo.frameCount = frameCount;
                ubo.model = glm::rotate(glm::mat4(1.0f), glm::radians(rotation), glm::vec3(0.0f, 0.0f, 1.0f));
                ubo.view = glm::lookAt(cameraPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
                ubo.proj = glm::perspective(glm::radians(60.0f), extent.width / (float)extent.height, 0.001f, 1000.0f);
                ubo.proj[1][1] *= -1;
                
                engine.fillBuffer(engine.getBuffer(uniformBuffers), (void*)&ubo);
                engine.getDescriptorSet(descriptorSets[frame]).bind(commandBuffer, pipeline.getLayout());
                
                pipeline.bind(commandBuffer);

                vertexBuffer.bindVertex(commandBuffer);
                indexBuffer.bindIndex(commandBuffer, VK_INDEX_TYPE_UINT16);
                
                pipeline.setViewport(commandBuffer, viewport);
                pipeline.setScissor(commandBuffer, scissor);
                pipeline.drawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()));

                engine.endDynamicRenderer(commandBuffer);
                engine.barrier(
                    commandBuffer,
                    images[frame].get(),
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                );
                engine.barrier(
                    commandBuffer,
                    images[1-frame].get(),
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_ACCESS_NONE, VK_ACCESS_SHADER_READ_BIT,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                );
            }
            
            {   // Screen
                engine.barrier(
                    commandBuffer,
                    nullptr,
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_ACCESS_NONE, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                );
                engine.beginDynamicRenderer(
                    commandBuffer,
                    nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
                    {{ 0.0f, 0.0f, 0.0f, 1.0f }}
                );

                engine.getDescriptorSet(screenDescriptorSets[frame]).bind(commandBuffer, screenPipeline.getLayout());

                screenPipeline.bind(commandBuffer);
                
                screenVertexBuffer.bindVertex(commandBuffer);
                indexBuffer.bindIndex(commandBuffer, VK_INDEX_TYPE_UINT16);
                
                screenPipeline.setViewport(commandBuffer, viewport);
                screenPipeline.setScissor(commandBuffer, scissor);
                screenPipeline.drawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()));

                engine.endDynamicRenderer(commandBuffer);
                engine.barrier(
                    commandBuffer,
                    nullptr,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_NONE,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
                );
            }
        }
        engine.endRecoringRender(commandBuffer);
            

        // TODO: might set default barrier and dyamic rendering context (at least for the UI)
        commandBuffer = engine.beginRecordingUiRender();
        {
            engine.barrier(
                commandBuffer,
                nullptr,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_ACCESS_NONE, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
            );
            engine.beginDynamicRenderer(
                commandBuffer,
                nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE,
                {{ 1.0f, 0.0f, 1.0f, 1.0f }}
            );
            
            drawUI(commandBuffer);

            engine.endDynamicRenderer(commandBuffer);
            engine.barrier(
                commandBuffer,
                nullptr,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_NONE,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
            );
        }
        engine.endRecoringUiRender(commandBuffer);

        engine.endFrame();
    }
}

void Application::drawUI(CommandBuffer commandBuffer) {
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

    ImGui::Begin("Control");
    {
        ImGui::Text("Camera Position");
    
        ImGui::PushItemWidth(-FLT_MIN);
        ImGui::DragFloat3("##Camera Position", glm::value_ptr(cameraPos), 0.01f, -10.0f, 10.0f, "%.2f");
        ImGui::PopItemWidth();
        
        ImGui::Text("Rotation Speed");
        
        ImGui::PushItemWidth(-FLT_MIN);
        ImGui::DragFloat("##Rotation Speed", &rotationSpeed, 1.0f, -180.0f, 180.0f, "%.f°/s");
        ImGui::PopItemWidth();
        
        ImGui::Separator();
        
        ImGui::PushItemWidth(-FLT_MIN);
        if (ImGui::Button("Reset Accumulation", ImVec2(ImGui::GetContentRegionAvail().x, 0))) frameCount = 0;
        ImGui::PopItemWidth();
    }
    ImGui::End();
    
    ImGui::Render();
    
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer.get());
}
