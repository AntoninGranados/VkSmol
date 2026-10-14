#pragma once

#include <vulkan/vulkan.h>

#include <vector>

namespace VkSmol {

class LogicalDevice;
class Shader;
class CommandBuffer;
class DescriptorSetLayout;
class PipelineCache;

class GraphicsPipeline {
public:
    GraphicsPipeline() {};

    void init(
        const LogicalDevice& device,
        VkPipelineRenderingCreateInfoKHR pipelineCreate,
        VkPipelineVertexInputStateCreateInfo vertexInputInfo,
        const std::vector<const Shader*>& shaders,
        const std::vector<const DescriptorSetLayout*>& setLayouts,
        uint32_t pushConstantSize,
        VkShaderStageFlags pushConstantStages,
        const PipelineCache& pipelineCache,
        GraphicsPipeline other = GraphicsPipeline()
        // VkViewport viewport, VkRect2D scissor,
    );
    void destroy(const LogicalDevice& device);
    VkPipeline get() { return pipeline; };
    VkPipelineLayout getLayout() { return pipelineLayout; };

    void bind(const CommandBuffer& commandBuffer);
    void setViewport(const CommandBuffer& commandBuffer, VkViewport viewport); // In case it is set to be dynamic
    void setScissor(const CommandBuffer& commandBuffer, VkRect2D scissor);     // In case it is set to be dynamic

    void draw(const CommandBuffer& commandBuffer, size_t vertexCount);
    void drawIndexed(const CommandBuffer& commandBuffer, size_t indexCount);

private:
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
};

}
