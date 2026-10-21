#include "compute_pipeline.hpp"

#include "../../command/command_buffer.hpp"
#include "../descriptor/descriptor_set_layout.hpp"
#include "../../device/logical_device.hpp"
#include "../shader.hpp"
#include "../../utils.hpp"
#include "./pipeline_cache.hpp"

namespace VkSmol {

void ComputePipeline::init(
    const LogicalDevice& device,
    const Shader& shader,
    const std::vector<const DescriptorSetLayout*>& setLayouts,
    uint32_t pushConstantSize,
    const PipelineCache& pipelineCache,
    ComputePipeline other
) {
    std::vector<VkDescriptorSetLayout> vkSetLayouts;
    for (const DescriptorSetLayout* setLayout : setLayouts)
        vkSetLayouts.emplace_back(setLayout->get());

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(vkSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = vkSetLayouts.data();
    const VkPushConstantRange pushConstantRange{
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset     = 0,
        .size       = pushConstantSize,
    };
    if (pushConstantSize > 0) {
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    }
    vkCheck(vkCreatePipelineLayout(device.get(), &pipelineLayoutInfo, nullptr, &pipelineLayout), "Failed to create compute pipeline layout");

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = shader.getShaderStageCreateInfo();
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.flags |= VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
    if (other.get() != VK_NULL_HANDLE) {
        pipelineInfo.basePipelineIndex = -1;
        pipelineInfo.basePipelineHandle = other.get();
        pipelineInfo.flags |= VK_PIPELINE_CREATE_DERIVATIVE_BIT;
    }

    vkCheck(vkCreateComputePipelines(device.get(), pipelineCache.get(), 1, &pipelineInfo, nullptr, &pipeline), "Failed to create compute pipeline");
}

void ComputePipeline::destroy(const LogicalDevice& device) {
    vkDestroyPipeline(device.get(), pipeline, nullptr);
    vkDestroyPipelineLayout(device.get(), pipelineLayout, nullptr);
}

void ComputePipeline::bind(const CommandBuffer& commandBuffer) const {
    vkCmdBindPipeline(commandBuffer.get(), VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
}

void ComputePipeline::dispatch(const CommandBuffer& commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) const {
    vkCmdDispatch(commandBuffer.get(), groupCountX, groupCountY, groupCountZ);
}

}
