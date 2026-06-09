#pragma once

#include <vulkan/vulkan.h>

#include <vector>

#include "builder_resource.hpp"

struct PassImageBarrier {
    ImageHandle image;
    ImageAccessInfo srcAccess;
    ImageAccessInfo dstAccess;
};

struct VulkanImageState {
    VkImageLayout layout;
    VkAccessFlags access;
    VkPipelineStageFlags stage;
    VkImageAspectFlags aspect;
};

struct CompiledPass {
    PassHandle pass;
    std::vector<PassImageBarrier> barriersBefore = {};
    std::vector<ImageAccess> imageAccesses = {};
};

struct CompiledSubmissionGroup {
    SubmissionGroupHandle handle;
    std::string name;
    std::vector<PassHandle> passes;
    std::vector<SubmissionGroupHandle> dependencies;  // groups this must wait on (in execution order)
    bool waitsOnImageAvailable = false;
    VkPipelineStageFlags imageAvailableWaitStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    bool signalsPresentSemaphore = false;
};

struct CompiledRenderGraph {
    std::vector<CompiledPass> compiledPasses = {};
    std::vector<PassImageBarrier> outputBarriers = {};
    std::vector<ImageInfo> images = {};
    std::vector<CompiledSubmissionGroup> submissionGroups = {};
};
