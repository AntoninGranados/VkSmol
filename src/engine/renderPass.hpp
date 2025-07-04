#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>

class LogicalDevice;
class CommandBuffer;
class Swapchain;

// TODO: add other attachements
struct SubpassInfo {
    std::vector<VkAttachmentReference> colorAttachmentRefs;
};

class RenderPass {
public:
    void init(LogicalDevice device);
    void destroy(LogicalDevice device);
    VkRenderPass get() { return renderPass; };

    // Needs to be called befor init()
    void addAttachment(VkAttachmentDescription attachment, VkImageLayout subpassLayout);
    // Needs to be called befor init()
    void addSubpass(
        VkPipelineBindPoint bindPoint,
        std::vector<size_t> colorAttachmentIndices
    );  //! Only supports color attachements for now
    // Needs to be called befor init()
    void addDepedency(
        uint32_t srcSubpass, uint32_t dstSubpass,
        VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask,
        VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask
    );

    void begin(CommandBuffer commandBuffer, Swapchain swapchain, VkFramebuffer frame, VkClearValue clearValue);
    void end(CommandBuffer commandBuffer);

private:
    VkRenderPass renderPass;
    std::vector<VkAttachmentDescription> attachments;
    std::vector<VkImageLayout> subpassLayouts;

    std::vector<SubpassInfo> subpasses;
    std::vector<VkPipelineBindPoint> subpassBindPoints;
    std::vector<VkSubpassDescription> subpasseDescriptions;

    std::vector<VkSubpassDependency> dependencies;

    VkAttachmentReference getAttachmentRef(size_t attachmentIndex);
    VkSubpassDescription getSubpassDescription(size_t subpassIndex);
};
