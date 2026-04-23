#pragma once

#include <vulkan/vulkan.h>

#include <vector>

#include "builder_resource.hpp"

struct PassImageBarrier {
    ImageHandle image;
    ImageAccessInfo srcAccess;
    ImageAccessInfo dstAccess;
};

struct PassDependency {
    PassHandle src;
    PassHandle dst;
    PassImageBarrier imageBarrier;
};

struct CompiledRenderGraph {
    std::vector<PassDependency> passDependencies;
};
