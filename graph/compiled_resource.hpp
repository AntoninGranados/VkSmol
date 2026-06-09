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

struct CompiledRenderGraph {
    std::vector<CompiledPass> compiledPasses = {};
    std::vector<PassImageBarrier> outputBarriers = {};
    std::vector<ImageInfo> images = {};
};
