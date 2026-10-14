#include "dynamic_renderer.hpp"

#include "../command/command_buffer.hpp"
#include "../present/swapchain.hpp"
#include "../device/logical_device.hpp"

namespace VkSmol {

void DynamicRenderer::init(const LogicalDevice& device) {
    cmdBeginRendering = device.functions().cmdBeginRendering;
    cmdEndRendering = device.functions().cmdEndRendering;
}

void DynamicRenderer::begin(
    const CommandBuffer& commandBuffer, 
    VkImageView imageView, VkImageLayout imageLayout,
    VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp,
    VkClearColorValue clearColor, VkRect2D renderArea
) {
    VkRenderingAttachmentInfoKHR colorAttachmentInfo{};
    colorAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    colorAttachmentInfo.imageView = imageView;
    colorAttachmentInfo.imageLayout = imageLayout;
    colorAttachmentInfo.loadOp = loadOp;
    colorAttachmentInfo.storeOp = storeOp;
    colorAttachmentInfo.clearValue.color = clearColor;
    
    VkRenderingInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.renderArea = renderArea;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &colorAttachmentInfo;
    renderInfo.layerCount = 1;
    
    cmdBeginRendering(commandBuffer.get(), &renderInfo);
}

void DynamicRenderer::end(const CommandBuffer& commandBuffer) {
    cmdEndRendering(commandBuffer.get());
}

}
