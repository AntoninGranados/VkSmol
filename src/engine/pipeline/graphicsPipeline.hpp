#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>

class LogicalDevice;
class Shader;
class CommandBuffer;
class DescriptorSetLayout;

// TODO: make it work with component (there are default values, but we can add/change components)
// TODO: keep track of the shader modules (in case we want to rebuild the pipeline with the same ones)
class GraphicsPipeline {
public:
    void init(
        LogicalDevice device,
        VkPipelineRenderingCreateInfoKHR pipelineCreate,
        VkPipelineVertexInputStateCreateInfo vertexInputInfo,
        std::vector<Shader> shaders,
        std::vector<DescriptorSetLayout> setLayouts
        // VkViewport viewport, VkRect2D scissor,
    );
    void destroy(LogicalDevice device);
    VkPipeline get() { return m_Pipeline; };
    VkPipelineLayout getLayout() { return m_PipelineLayout; };

    void bind(CommandBuffer commandBuffer);
    void setViewport(CommandBuffer commandBuffer, VkViewport viewport); // In case it is set to be dynamic
    void setScissor(CommandBuffer commandBuffer, VkRect2D scissor);     // In case it is set to be dynamic
    
    void draw(CommandBuffer commandBuffer, size_t vertexCount);
    void drawIndexed(CommandBuffer commandBuffer, size_t indexCount);

private:
    VkPipeline m_Pipeline;
    VkPipelineLayout m_PipelineLayout;
};
