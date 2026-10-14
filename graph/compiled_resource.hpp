#pragma once

#include <vulkan/vulkan.h>

#include <vector>
#include <optional>

#include "builder_resource.hpp"

namespace VkSmol {

struct PassImageBarrier {
    ImageHandle image;
    ImageAccessInfo srcAccess;
    ImageAccessInfo dstAccess;
};

struct PassBufferBarrier {
    BufferHandle buffer;
    BufferAccessInfo srcAccess;
    BufferAccessInfo dstAccess;
};

struct VulkanImageState {
    VkImageLayout layout;
    VkAccessFlags2 access;
    VkPipelineStageFlags2 stage;
    VkImageAspectFlags aspect;
};

struct VulkanBufferState {
    VkAccessFlags2 access;
    VkPipelineStageFlags2 stage;
};

struct CompiledPass {
    PassHandle pass;
    std::string name;
    PassType    type = PassType::Compute;
    std::vector<PassImageBarrier>      barriersBefore       = {};
    std::vector<PassBufferBarrier>     bufferBarriersBefore = {};
    std::vector<ImageAccess>           imageAccesses        = {};
    std::vector<BufferAccess>          bufferAccesses       = {};
    std::vector<DescriptorBindingDecl> descriptorBindings   = {};
    std::optional<uint32_t>            pipelineId;
};

struct CompiledRenderGraph {
    std::vector<CompiledPass>            compiledPasses   = {};
    std::vector<PassImageBarrier>        outputBarriers   = {};
    std::vector<ImageInfo>               images           = {};
    std::vector<BufferInfo>              buffers          = {};
    std::vector<ComputePipelineInfo>     computePipelines = {};
    std::vector<GraphicsPipelineInfo>    graphicsPipelines = {};
};

}
