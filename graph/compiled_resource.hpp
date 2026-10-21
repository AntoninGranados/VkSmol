#pragma once

#include <vulkan/vulkan.h>

#include <vector>
#include <optional>

#include "builder_resource.hpp"

namespace VkSmol {

struct VulkanImageState {
    VkImageLayout layout;
    VkAccessFlags2 access;
    VkPipelineStageFlags2 stage;
};

struct VulkanBufferState {
    VkAccessFlags2 access;
    VkPipelineStageFlags2 stage;
};

struct CompiledPass {
    PassHandle pass;
    std::string name;
    PassType    type = PassType::Compute;
    std::vector<ImageAccess>           imageAccesses      = {};
    std::vector<BufferAccess>          bufferAccesses     = {};
    std::vector<DescriptorBindingDecl> descriptorBindings = {};
    std::optional<uint32_t>            pipelineId;
};

struct CompiledRenderGraph {
    std::vector<CompiledPass>         compiledPasses    = {};
    std::vector<ComputePipelineInfo>  computePipelines  = {};
    std::vector<GraphicsPipelineInfo> graphicsPipelines = {};
};

}
