#pragma once

#include <vector>

#include "./engine/engine.hpp"

typedef uint16_t index_t;

struct Vertex {
    glm::vec2 position;
    glm::vec3 color;
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

    DescriptorSetLayout setLayout;
    descriptorSetList_t descriptorSets;
    GraphicsPipeline pipeline;

    Buffer vertexBuffer, indexBuffer;
    bufferList_t uniformBuffers;
    

};
