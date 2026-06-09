#include "graphics_pass_builder.hpp"

#include "../graph_utils.hpp"
#include "../builder_resource.hpp"
#include "../render_graph_builder.hpp"

static constexpr VkShaderStageFlags kGraphicsStage = VK_SHADER_STAGE_ALL_GRAPHICS;

void GraphicsPassBuilder::readImage(ImageHandle imageHandle, ImageUsageType usage) {
    checkImageAccessInfoValid({
        .usage = usage,
        .access = AccessType::Read,
    });

    graph->passes[handle.id].imageAccesses.push_back({
        .image = imageHandle,
        .info = {
            .usage = usage,
            .access = AccessType::Read,
        },
    });
}

void GraphicsPassBuilder::writeImage(
    ImageHandle imageHandle,
    ImageUsageType usage,
    WriteMode writeMode,
    AttachmentLoad attachmentLoad
) {
    checkImageAccessInfoValid({
        .usage = usage,
        .access = AccessType::Write,
        .writeMode = writeMode,
        .attachmentLoad = attachmentLoad,
    });

    graph->passes[handle.id].imageAccesses.push_back({
        .image = imageHandle,
        .info = {
            .usage = usage,
            .access = AccessType::Write,
            .writeMode = writeMode,
            .attachmentLoad = attachmentLoad,
        },
    });
}

void GraphicsPassBuilder::readBuffer(BufferHandle buffer, BufferUsageType usage) {
    graph->passes[handle.id].bufferAccesses.push_back({
        .buffer = buffer,
        .info = { .usage = usage, .access = AccessType::Read },
    });
}

void GraphicsPassBuilder::writeBuffer(BufferHandle buffer, BufferUsageType usage) {
    graph->passes[handle.id].bufferAccesses.push_back({
        .buffer = buffer,
        .info = { .usage = usage, .access = AccessType::Write },
    });
}

void GraphicsPassBuilder::readImage(ImageHandle imageHandle, ImageUsageType usage, uint32_t binding) {
    readImage(imageHandle, usage);
    VkDescriptorType dt = imageUsageTypeToDescriptorType(usage);
    if (dt != VK_DESCRIPTOR_TYPE_MAX_ENUM)
        graph->passes[handle.id].descriptorBindings.push_back({ binding, true, imageHandle.id, dt, kGraphicsStage });
}

void GraphicsPassBuilder::readBuffer(BufferHandle buffer, BufferUsageType usage, uint32_t binding) {
    readBuffer(buffer, usage);
    VkDescriptorType dt = bufferUsageTypeToDescriptorType(usage);
    if (dt != VK_DESCRIPTOR_TYPE_MAX_ENUM)
        graph->passes[handle.id].descriptorBindings.push_back({ binding, false, buffer.id, dt, kGraphicsStage });
}

GraphicsPipelineHandle GraphicsPassBuilder::setPipeline(VkPipelineVertexInputStateCreateInfo vi, std::vector<std::pair<VkShaderStageFlagBits, std::string>> shaders) {
    GraphicsPipelineHandle h = { .id = static_cast<uint32_t>(graph->graphicsPipelines.size()) };
    GraphicsPipelineInfo info;
    info.name        = graph->passes[handle.id].name + "Pipeline";
    info.shaderPaths = std::move(shaders);
    info.passId      = handle.id;
    if (vi.pVertexBindingDescriptions)
        info.bindingDescription = *vi.pVertexBindingDescriptions;
    for (uint32_t i = 0; i < vi.vertexAttributeDescriptionCount; ++i)
        info.attributeDescriptions.push_back(vi.pVertexAttributeDescriptions[i]);
    graph->graphicsPipelines.push_back(std::move(info));
    return h;
}
