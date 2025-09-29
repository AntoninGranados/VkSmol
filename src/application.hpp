#pragma once

#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "./engine/engine.hpp"

typedef uint16_t index_t;

struct Vertex {
    glm::vec2 position;
    glm::vec3 color;
    glm::vec2 texCoord;
};

struct ScreenVertex {
    glm::vec2 position;
};

struct UBO {
    float time;
    int frameCount;
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

class Application {
public:
    Application();
    ~Application();

    void run();

private:
    VkSmol engine;

    Image images[2];
    ImageView imageViews[2];
    Sampler samplers[2];
    
    DescriptorSetLayout setLayout, screenSetLayout;
    descriptorSetList_t descriptorSets[2], screenDescriptorSets[2];
    GraphicsPipeline pipeline, screenPipeline;
    
    Buffer vertexBuffer, screenVertexBuffer, indexBuffer;
    bufferList_t uniformBuffers;
    
    size_t frame = 0;
    int frameCount = 0;

    glm::vec3 cameraPos = { 0.0f, 1.0f, 2.0f };
    float rotationSpeed = 0.0f;
    float rotation = 0.0f;

    void drawUI(CommandBuffer commandBuffer);
};
