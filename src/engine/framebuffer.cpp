#include "framebuffer.hpp"

#include "./logicalDevice.hpp"
#include "./swapchain.hpp"
#include "./renderPass.hpp"
#include "./utils.hpp"

void Framebuffer::init(LogicalDevice device, Swapchain swapchain, RenderPass renderPass) {
    framebuffers.resize(swapchain.getImageCount());

    for (size_t i = 0; i < swapchain.getImageCount(); i++) {
        VkImageView attachments[] = { swapchain.getImageView(i) };

        VkFramebufferCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfo.renderPass = renderPass.get();
        createInfo.attachmentCount = 1;
        createInfo.pAttachments = attachments;
        createInfo.width = swapchain.getExtent().width;
        createInfo.height = swapchain.getExtent().height;
        createInfo.layers = 1;

        vkCheck(vkCreateFramebuffer(device.get(), &createInfo, nullptr, &framebuffers[i]), "Failed to create framebuffer");
    }
}

void Framebuffer::destroy(LogicalDevice device) {
    for (VkFramebuffer framebuffer : framebuffers)
        vkDestroyFramebuffer(device.get(), framebuffer, nullptr);

    framebuffers.clear();
}
