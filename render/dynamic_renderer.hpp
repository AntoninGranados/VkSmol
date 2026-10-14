#pragma once

#include <vulkan/vulkan.h>

namespace VkSmol {

class CommandBuffer;
class Swapchain;
class LogicalDevice;

class DynamicRenderer {
public:
    void init(const LogicalDevice& device);
    
    void begin(
        const CommandBuffer& commandBuffer,
        VkImageView imageView, VkImageLayout imageLayout,
        VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp,
        VkClearColorValue clearColor, VkRect2D renderArea
    );
    void end(const CommandBuffer& commandBuffer);

private:
    PFN_vkCmdBeginRenderingKHR cmdBeginRendering = nullptr;
    PFN_vkCmdEndRenderingKHR cmdEndRendering = nullptr;
};

}
