#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "../render/pipeline/compute_pipeline.hpp"
#include "../render/pipeline/graphics_pipeline.hpp"
#include "../render/pipeline/pipeline_cache.hpp"
#include "../render/shader.hpp"
#include "builder_resource.hpp"

namespace VkSmol {

class CommandBuffer;
class Device;
class LogicalDevice;
class PassDescriptors;
class ResourceRegistry;

class PassPipelines {
public:
    PassPipelines(Device& device, ResourceRegistry& resources);

    void init(const GraphDeclaration& graph, const PassDescriptors& descriptors, const std::string& cachePath);
    void destroy();

    void bind(const CommandBuffer& commandBuffer, const PassInfo& pass);
    VkPipelineLayout getLayout(const PassInfo& pass);
    void setRenderArea(const CommandBuffer& commandBuffer, const PassInfo& pass, VkExtent2D extent);
    void dispatch(const CommandBuffer& commandBuffer, const PassInfo& pass, uint32_t x, uint32_t y, uint32_t z);
    VkExtent3D getThreadGroupSize(const PassInfo& pass) const;
    void pushConstants(const CommandBuffer& commandBuffer, const PassInfo& pass, const void* data, uint32_t size);

private:
    struct Compute {
        ComputePipeline pipeline;
        uint32_t pushConstantSize = 0;
        VkExtent3D threadGroupSize = {1, 1, 1};
    };

    struct Graphics {
        GraphicsPipeline pipeline;
        uint32_t pushConstantSize = 0;
        VkShaderStageFlags pushConstantStages = 0;
    };

    Device& device;
    const LogicalDevice& logicalDevice;
    ResourceRegistry& resources;

    PipelineCache cache;
    std::unordered_map<uint32_t, Compute> computes;
    std::unordered_map<uint32_t, Graphics> graphics;

    void createComputePipelines(const GraphDeclaration& graph, const PassDescriptors& descriptors);
    void createGraphicsPipelines(const GraphDeclaration& graph, const PassDescriptors& descriptors);
    const ShaderCompilation& compileProgram(
        const PassInfo& pass, const std::vector<std::pair<VkShaderStageFlagBits, std::string>>& stages,
        const ShaderDefines& defines, const ShaderConformances& conformances
    );
};

} // namespace VkSmol
