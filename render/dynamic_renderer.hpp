#pragma once

#include <vulkan/vulkan.h>

#include <vector>

namespace VkSmol {

class CommandBuffer;
class LogicalDevice;

class DynamicRenderer {
public:
    void init(const LogicalDevice& device);

    void begin(
        const CommandBuffer& commandBuffer, const std::vector<VkRenderingAttachmentInfo>& colorAttachments,
        const VkRenderingAttachmentInfo* depthAttachment, VkRect2D renderArea
    );
    void end(const CommandBuffer& commandBuffer);

private:
    PFN_vkCmdBeginRenderingKHR cmdBeginRendering = nullptr;
    PFN_vkCmdEndRenderingKHR cmdEndRendering = nullptr;
};

}
