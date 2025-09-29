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

struct UBO {
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

    Image image;
    ImageView imageView;
    Sampler sampler;

    DescriptorSetLayout setLayout;
    descriptorSetList_t descriptorSets;
    GraphicsPipeline pipeline;

    Buffer vertexBuffer, indexBuffer;
    bufferList_t uniformBuffers;
    
    glm::vec3 cameraPos = { 0.0f, 1.0f, 2.0f };
    float rotationSpeed = 0.0f;
    float rotation = 0.0f;
};
