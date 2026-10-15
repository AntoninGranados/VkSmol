#pragma once

#include <vulkan/vulkan.h>

#include <optional>
#include <vector>

namespace VkSmol {

struct PrimitiveState {
    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
    VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
};

struct DepthState {
    bool test = true;
    bool write = true;
    VkCompareOp compare = VK_COMPARE_OP_LESS;
};

struct BlendState {
    bool enable = false;
    VkBlendFactor sourceColor = VK_BLEND_FACTOR_ONE;
    VkBlendFactor destinationColor = VK_BLEND_FACTOR_ZERO;
    VkBlendOp colorOperation = VK_BLEND_OP_ADD;
    VkBlendFactor sourceAlpha = VK_BLEND_FACTOR_ONE;
    VkBlendFactor destinationAlpha = VK_BLEND_FACTOR_ZERO;
    VkBlendOp alphaOperation = VK_BLEND_OP_ADD;
    VkColorComponentFlags writeMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    static BlendState opaque() { return {}; }

    static BlendState alpha() {
        return {
            .enable = true,
            .sourceColor = VK_BLEND_FACTOR_SRC_ALPHA,
            .destinationColor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .destinationAlpha = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        };
    }

    static BlendState premultiplied() {
        return {
            .enable = true,
            .destinationColor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .destinationAlpha = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        };
    }

    static BlendState additive() {
        return {.enable = true, .destinationColor = VK_BLEND_FACTOR_ONE, .destinationAlpha = VK_BLEND_FACTOR_ONE};
    }
};

struct GraphicsState {
    PrimitiveState primitive;
    std::optional<DepthState> depth;
    std::vector<BlendState> blend;
};

} // namespace VkSmol
