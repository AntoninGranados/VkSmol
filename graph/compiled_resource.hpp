#pragma once

#include <vulkan/vulkan.h>

#include <vector>

#include "builder_resource.hpp"

struct ImageUsageRecord {
    PassHandle pass;
    ImageUsage usage;
    AccessType access;
};

struct ImageChain {
    ImageHandle image;
    std::vector<ImageUsageRecord> usages;

    PassHandle lastWriter = { .id = InvalidHandle };
};

struct PassDependency {
    PassHandle src;
    PassHandle dst;
    ImageHandle image;
};

struct CompiledRenderGraph {
    std::vector<ImageChain> imageChains;
    std::vector<PassDependency> passDependencies;
};
