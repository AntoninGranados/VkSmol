#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>

namespace VkSmol {

class LogicalDevice;
class Shader;
class CommandBuffer;
class DescriptorSetLayout;
class PipelineCache;

class ComputePipeline {
public:
    ComputePipeline() {};

    void init(
        const LogicalDevice& device,
        const Shader& shader,
        const std::vector<const DescriptorSetLayout*>& setLayouts,
        const PipelineCache& pipelineCache,
        ComputePipeline other = {}
    );
    void destroy(const LogicalDevice& device);

    VkPipeline get() const { return pipeline; }
    VkPipelineLayout getLayout() const { return pipelineLayout; }

    void bind(const CommandBuffer& commandBuffer) const;
    void dispatch(const CommandBuffer& commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ = 1) const;

private:
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
};

}
