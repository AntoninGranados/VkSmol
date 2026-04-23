#pragma once

#include <vulkan/vulkan.h>

#include <vector>

#include "builder_resource.hpp"

struct PassImageBarrier {
    ImageHandle image;
    ImageAccessInfo srcAccess;
    ImageAccessInfo dstAccess;
};

struct CompiledPass {
    PassHandle pass;
    std::vector<PassImageBarrier> barriersBefore;
};

struct CompiledRenderGraph {
    std::vector<CompiledPass> compiledPasses;
    std::vector<PassImageBarrier> outputBarriers;
};
