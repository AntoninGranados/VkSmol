#include "compute_pass_builder.hpp"

#include "../graph_utils.hpp"
#include "../builder_resource.hpp"
#include "../render_graph_builder.hpp"

static constexpr VkShaderStageFlags kComputeStage = VK_SHADER_STAGE_COMPUTE_BIT;

void ComputePassBuilder::readImage(uint32_t binding, ImageHandle imageHandle, ImageUsageType usage) {
    checkImageAccessInfoValid({ .usage = usage, .access = AccessType::Read });
    graph->passes[handle.id].imageAccesses.push_back({
        .image = imageHandle,
        .info  = { .usage = usage, .access = AccessType::Read },
    });
    VkDescriptorType dt = imageUsageTypeToDescriptorType(usage);
    if (dt != VK_DESCRIPTOR_TYPE_MAX_ENUM)
        graph->passes[handle.id].descriptorBindings.push_back({ binding, true, imageHandle.id, dt, kComputeStage });
}

void ComputePassBuilder::writeImage(uint32_t binding, ImageHandle imageHandle, ImageUsageType usage) {
    checkImageAccessInfoValid({ .usage = usage, .access = AccessType::Write });
    graph->passes[handle.id].imageAccesses.push_back({
        .image = imageHandle,
        .info  = { .usage = usage, .access = AccessType::Write },
    });
    VkDescriptorType dt = imageUsageTypeToDescriptorType(usage);
    if (dt != VK_DESCRIPTOR_TYPE_MAX_ENUM)
        graph->passes[handle.id].descriptorBindings.push_back({ binding, true, imageHandle.id, dt, kComputeStage });
}

void ComputePassBuilder::readBuffer(uint32_t binding, BufferHandle buffer, BufferUsageType usage) {
    graph->passes[handle.id].bufferAccesses.push_back({
        .buffer = buffer,
        .info   = { .usage = usage, .access = AccessType::Read },
    });
    VkDescriptorType dt = bufferUsageTypeToDescriptorType(usage);
    if (dt != VK_DESCRIPTOR_TYPE_MAX_ENUM)
        graph->passes[handle.id].descriptorBindings.push_back({ binding, false, buffer.id, dt, kComputeStage });
}

void ComputePassBuilder::writeBuffer(uint32_t binding, BufferHandle buffer, BufferUsageType usage) {
    graph->passes[handle.id].bufferAccesses.push_back({
        .buffer = buffer,
        .info   = { .usage = usage, .access = AccessType::Write },
    });
    VkDescriptorType dt = bufferUsageTypeToDescriptorType(usage);
    if (dt != VK_DESCRIPTOR_TYPE_MAX_ENUM)
        graph->passes[handle.id].descriptorBindings.push_back({ binding, false, buffer.id, dt, kComputeStage });
}

ComputePipelineHandle ComputePassBuilder::setPipeline(std::string shaderCode) {
    ComputePipelineHandle h = { .id = static_cast<uint32_t>(graph->computePipelines.size()) };
    graph->computePipelines.push_back({
        .name       = graph->passes[handle.id].name + "Pipeline",
        .shaderCode = std::move(shaderCode),
        .passId     = handle.id,
    });
    return h;
}
