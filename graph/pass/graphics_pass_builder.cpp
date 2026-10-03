#include "graphics_pass_builder.hpp"

#include "../graph_utils.hpp"
#include "../builder_resource.hpp"
#include "../render_graph_builder.hpp"

static constexpr VkShaderStageFlags kGraphicsStage = VK_SHADER_STAGE_ALL_GRAPHICS;

void GraphicsPassBuilder::readImage(uint32_t binding, ImageHandle imageHandle, ImageUsageType usage) {
    checkImageAccessInfoValid({ .usage = usage, .access = AccessType::Read });
    graph->passes[handle.id].imageAccesses.push_back({
        .image = imageHandle,
        .info  = { .usage = usage, .access = AccessType::Read },
    });
    VkDescriptorType dt = imageUsageTypeToDescriptorType(usage);
    if (dt != VK_DESCRIPTOR_TYPE_MAX_ENUM)
        graph->passes[handle.id].descriptorBindings.push_back({ binding, true, imageHandle.id, dt, kGraphicsStage });
}

void GraphicsPassBuilder::writeImage(
    ImageHandle imageHandle,
    ImageUsageType usage,
    WriteMode writeMode,
    AttachmentLoad attachmentLoad
) {
    checkImageAccessInfoValid({
        .usage          = usage,
        .access         = AccessType::Write,
        .writeMode      = writeMode,
        .attachmentLoad = attachmentLoad,
    });
    graph->passes[handle.id].imageAccesses.push_back({
        .image = imageHandle,
        .info  = {
            .usage          = usage,
            .access         = AccessType::Write,
            .writeMode      = writeMode,
            .attachmentLoad = attachmentLoad,
        },
    });
}

void GraphicsPassBuilder::readBuffer(uint32_t binding, BufferHandle buffer, BufferUsageType usage) {
    graph->passes[handle.id].bufferAccesses.push_back({
        .buffer = buffer,
        .info   = { .usage = usage, .access = AccessType::Read },
    });
    VkDescriptorType dt = bufferUsageTypeToDescriptorType(usage);
    if (dt != VK_DESCRIPTOR_TYPE_MAX_ENUM)
        graph->passes[handle.id].descriptorBindings.push_back({ binding, false, buffer.id, dt, kGraphicsStage });
}

void GraphicsPassBuilder::writeBuffer(uint32_t binding, BufferHandle buffer, BufferUsageType usage) {
    graph->passes[handle.id].bufferAccesses.push_back({
        .buffer = buffer,
        .info   = { .usage = usage, .access = AccessType::Write },
    });
    VkDescriptorType dt = bufferUsageTypeToDescriptorType(usage);
    if (dt != VK_DESCRIPTOR_TYPE_MAX_ENUM)
        graph->passes[handle.id].descriptorBindings.push_back({ binding, false, buffer.id, dt, kGraphicsStage });
}

void GraphicsPassBuilder::bindVertexBuffer(BufferHandle buffer) {
    graph->passes[handle.id].bufferAccesses.push_back({
        .buffer = buffer,
        .info   = { .usage = BufferUsageType::Vertex, .access = AccessType::Read },
    });
}

void GraphicsPassBuilder::bindIndexBuffer(BufferHandle buffer) {
    graph->passes[handle.id].bufferAccesses.push_back({
        .buffer = buffer,
        .info   = { .usage = BufferUsageType::Index, .access = AccessType::Read },
    });
}

GraphicsPipelineHandle GraphicsPassBuilder::setPipeline(VkPipelineVertexInputStateCreateInfo vi, std::vector<std::pair<VkShaderStageFlagBits, std::string>> shaders) {
    GraphicsPipelineHandle h = { .id = static_cast<uint32_t>(graph->graphicsPipelines.size()) };
    GraphicsPipelineInfo info;
    info.name        = graph->passes[handle.id].name + "Pipeline";
    info.shaderCodes = std::move(shaders);
    info.passId      = handle.id;
    if (vi.pVertexBindingDescriptions)
        info.bindingDescription = *vi.pVertexBindingDescriptions;
    for (uint32_t i = 0; i < vi.vertexAttributeDescriptionCount; ++i)
        info.attributeDescriptions.push_back(vi.pVertexAttributeDescriptions[i]);
    graph->graphicsPipelines.push_back(std::move(info));
    return h;
}
