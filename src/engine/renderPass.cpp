#include "renderPass.hpp"

#include "./command/commandBuffer.hpp"
#include "./logicalDevice.hpp"
#include "./swapchain.hpp"
#include "./utils.hpp"

void RenderPass::init(LogicalDevice device) {
    for (size_t i = 0; i < subpasses.size(); i++)
        subpasseDescriptions.push_back(getSubpassDescription(i));

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = static_cast<uint32_t>(subpasseDescriptions.size());
    renderPassInfo.pSubpasses = subpasseDescriptions.data();

    vkCheck(vkCreateRenderPass(device.get(), &renderPassInfo, nullptr, &renderPass), "Failed to create render pass");
}

void RenderPass::destroy(LogicalDevice device) {
    vkDestroyRenderPass(device.get(), renderPass, nullptr);
}

void RenderPass::addAttachment(VkAttachmentDescription attachment, VkImageLayout subpassLayout) {
    attachments.push_back(attachment);
    subpassLayouts.push_back(subpassLayout);
}

void RenderPass::addSubpass(
    VkPipelineBindPoint bindPoint,
    std::vector<size_t> colorAttachmentIndices
) {
    subpassBindPoints.push_back(bindPoint);

    SubpassInfo subpass{};
    for (size_t idx : colorAttachmentIndices)
        subpass.colorAttachmentRefs.push_back(getAttachmentRef(idx));
    subpasses.push_back(subpass);
}

void RenderPass::begin(CommandBuffer commandBuffer, Swapchain swapchain, VkFramebuffer frame, VkClearValue clearValue) {
    VkRenderPassBeginInfo renderPassBegin{};
    renderPassBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBegin.renderPass = renderPass;
    renderPassBegin.framebuffer = frame;
    renderPassBegin.renderArea.offset = { 0, 0 };
    renderPassBegin.renderArea.extent = swapchain.getExtent();
    renderPassBegin.clearValueCount = 1;
    renderPassBegin.pClearValues = &clearValue;

    vkCmdBeginRenderPass(commandBuffer.get(), &renderPassBegin, VK_SUBPASS_CONTENTS_INLINE);
}

void RenderPass::end(CommandBuffer commandBuffer) {
    vkCmdEndRenderPass(commandBuffer.get());
}


VkAttachmentReference RenderPass::getAttachmentRef(size_t attachmentIndex) {
    if (attachmentIndex >= attachments.size())
        throw std::runtime_error("Could not find attachment in the render pass, be sure to add it before referencing it in the subpass");

    VkAttachmentReference attachmentRef{};
    attachmentRef.attachment = attachmentIndex;
    attachmentRef.layout = subpassLayouts[attachmentIndex];
    return attachmentRef;
}

VkSubpassDescription RenderPass::getSubpassDescription(size_t subpassIndex) {
    VkSubpassDescription subpasseDescription{};
    subpasseDescription.pipelineBindPoint = subpassBindPoints[subpassIndex];
    subpasseDescription.colorAttachmentCount = static_cast<uint32_t>(subpasses[subpassIndex].colorAttachmentRefs.size());
    subpasseDescription.pColorAttachments = subpasses[subpassIndex].colorAttachmentRefs.data();
    return subpasseDescription;
}
