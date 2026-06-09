#include "compute_pass_builder.hpp"

#include "../graph_utils.hpp"
#include "../builder_resource.hpp"
#include "../render_graph_builder.hpp"

static constexpr VkShaderStageFlags kComputeStage = VK_SHADER_STAGE_COMPUTE_BIT;

void ComputePassBuilder::readImage(ImageHandle imageHandle, ImageUsageType usage) {
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

void ComputePassBuilder::writeImage(ImageHandle imageHandle, ImageUsageType usage) {
    checkImageAccessInfoValid({
        .usage = usage,
        .access = AccessType::Write,
    });

    graph->passes[handle.id].imageAccesses.push_back({
        .image = imageHandle,
        .info = {
            .usage = usage,
            .access = AccessType::Write,
        },
    });
}

void ComputePassBuilder::readBuffer(BufferHandle buffer, BufferUsageType usage) {
    graph->passes[handle.id].bufferAccesses.push_back({
        .buffer = buffer,
        .info = { .usage = usage, .access = AccessType::Read },
    });
}

void ComputePassBuilder::writeBuffer(BufferHandle buffer, BufferUsageType usage) {
    graph->passes[handle.id].bufferAccesses.push_back({
        .buffer = buffer,
        .info = { .usage = usage, .access = AccessType::Write },
    });
}

void ComputePassBuilder::readImage(ImageHandle imageHandle, ImageUsageType usage, uint32_t binding) {
    readImage(imageHandle, usage);
    VkDescriptorType dt = imageUsageTypeToDescriptorType(usage);
    if (dt != VK_DESCRIPTOR_TYPE_MAX_ENUM)
        graph->passes[handle.id].descriptorBindings.push_back({ binding, true, imageHandle.id, dt, kComputeStage });
}

void ComputePassBuilder::writeImage(ImageHandle imageHandle, ImageUsageType usage, uint32_t binding) {
    writeImage(imageHandle, usage);
    VkDescriptorType dt = imageUsageTypeToDescriptorType(usage);
    if (dt != VK_DESCRIPTOR_TYPE_MAX_ENUM)
        graph->passes[handle.id].descriptorBindings.push_back({ binding, true, imageHandle.id, dt, kComputeStage });
}

void ComputePassBuilder::readBuffer(BufferHandle buffer, BufferUsageType usage, uint32_t binding) {
    readBuffer(buffer, usage);
    VkDescriptorType dt = bufferUsageTypeToDescriptorType(usage);
    if (dt != VK_DESCRIPTOR_TYPE_MAX_ENUM)
        graph->passes[handle.id].descriptorBindings.push_back({ binding, false, buffer.id, dt, kComputeStage });
}

ComputePipelineHandle ComputePassBuilder::setPipeline(std::string shaderPath) {
    ComputePipelineHandle h = { .id = static_cast<uint32_t>(graph->computePipelines.size()) };
    graph->computePipelines.push_back({
        .name       = graph->passes[handle.id].name + "Pipeline",
        .shaderPath = std::move(shaderPath),
        .passId     = handle.id,
    });
    return h;
}
