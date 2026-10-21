#pragma once

#include <vulkan/vulkan.h>

#include <optional>

#include "imgui/imgui.h"

#include "../graph/builder_resource.hpp"
#include "../render/descriptor/descriptor_pool.hpp"

namespace VkSmol {

class Engine;
class Window;
class PassContext;

class ImGuiLayer {
public:
    ImGuiLayer(Engine& engine, Window& window, std::optional<VkFormat> colorFormat = std::nullopt);
    ~ImGuiLayer();

    ImGuiLayer(const ImGuiLayer&) = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;
    ImGuiLayer(ImGuiLayer&&) = delete;
    ImGuiLayer& operator=(ImGuiLayer&&) = delete;

    void newFrame();
    void render(PassContext& context);

    ImTextureID addTexture(VkSampler sampler, VkImageView view, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    ImTextureID addTexture(ImageRange image);
    void removeTexture(ImTextureID texture);

private:
    Engine& engine;
    DescriptorPool descriptorPool;
    VkFormat colorFormat;
};

}
