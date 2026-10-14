#pragma once

#include <vulkan/vulkan.h>

namespace VkSmol {

struct FrameContext {
    VkExtent2D extent;
    uint32_t currentFrame;
    uint32_t imageIndex;
    uint64_t swapchainGeneration;
};

}
