#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>

class LogicalDevice;
class Shader;
class RenderPass;

// TODO: make it work with component (there are default values, but we can add/change components)
// TODO: keep track of the shader modules (in case we want to rebuild the pipeline with the same ones)
class GraphicsPipeline {
public:
    void init(
        LogicalDevice device,
        RenderPass renderPass,
        std::vector<Shader> shaders
        // VkViewport viewport, VkRect2D scissor,
    );
    void destroy(LogicalDevice device);

private:
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
};
