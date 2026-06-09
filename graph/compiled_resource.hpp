#pragma once

#include <vulkan/vulkan.h>

#include <vector>

#include "builder_resource.hpp"

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
    VkAccessFlags access;
    VkPipelineStageFlags stage;
    VkImageAspectFlags aspect;
};

struct VulkanBufferState {
    VkAccessFlags access;
    VkPipelineStageFlags stage;
};

struct CompiledPass {
    PassHandle pass;
    std::vector<PassImageBarrier>  barriersBefore       = {};
    std::vector<PassBufferBarrier> bufferBarriersBefore = {};
    std::vector<ImageAccess>  imageAccesses  = {};
    std::vector<BufferAccess> bufferAccesses = {};
};

struct CompiledSubmissionGroup {
    SubmissionGroupHandle handle;
    std::string name;
    std::vector<PassHandle> passes;
    std::vector<SubmissionGroupHandle> dependencies;
    bool waitsOnImageAvailable = false;
    VkPipelineStageFlags imageAvailableWaitStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    bool signalsPresentSemaphore = false;
};

struct CompiledRenderGraph {
    std::vector<CompiledPass>            compiledPasses   = {};
    std::vector<PassImageBarrier>        outputBarriers   = {};
    std::vector<ImageInfo>               images           = {};
    std::vector<BufferInfo>              buffers          = {};
    std::vector<ComputePipelineInfo>     computePipelines = {};
    std::vector<GraphicsPipelineInfo>    graphicsPipelines = {};
    std::vector<CompiledSubmissionGroup> submissionGroups = {};
};
