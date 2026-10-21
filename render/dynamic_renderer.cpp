#include "dynamic_renderer.hpp"

#include "../command/command_buffer.hpp"
#include "../device/logical_device.hpp"

namespace VkSmol {

void DynamicRenderer::init(const LogicalDevice& device) {
    cmdBeginRendering = device.functions().cmdBeginRendering;
    cmdEndRendering = device.functions().cmdEndRendering;
}

void DynamicRenderer::begin(
    const CommandBuffer& commandBuffer, const std::vector<VkRenderingAttachmentInfo>& colorAttachments,
    const VkRenderingAttachmentInfo* depthAttachment, VkRect2D renderArea
) {
    const VkRenderingInfo renderInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = renderArea,
        .layerCount = 1,
        .colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size()),
        .pColorAttachments = colorAttachments.data(),
        .pDepthAttachment = depthAttachment,
    };
    cmdBeginRendering(commandBuffer.get(), &renderInfo);
}

void DynamicRenderer::end(const CommandBuffer& commandBuffer) {
    cmdEndRendering(commandBuffer.get());
}

}
