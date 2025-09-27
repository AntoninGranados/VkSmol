#include "descriptorSet.hpp"

#include "./descriptorSetLayout.hpp"
#include "./descriptorPool.hpp"
#include "../logicalDevice.hpp"
#include "../memory/buffer.hpp"
#include "../command/commandBuffer.hpp"
#include "../utils.hpp"

void DescriptorSet::alloc(LogicalDevice device, DescriptorPool descriptorPool, DescriptorSetLayout descriptorSetLayout) {
    VkDescriptorSetLayout vkDescriptorSetLayout = descriptorSetLayout.get();

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool.get();
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &vkDescriptorSetLayout;

    vkCheck(vkAllocateDescriptorSets(device.get(), &allocInfo, &descriptorSet), "Failed to allocate descriptor set");
}

void DescriptorSet::alloc(
    std::vector<DescriptorSet>& descriptorSets,
    LogicalDevice device,
    DescriptorPool descriptorPool,
    std::vector<DescriptorSetLayout> descriptorSetLayouts
) {
    std::vector<VkDescriptorSet> vkDescriptorSets;
    for (DescriptorSet& descriptorSet : descriptorSets)
        vkDescriptorSets.emplace_back(descriptorSet.get());
    
        std::vector<VkDescriptorSetLayout> vkDescriptorSetLayouts;
    for (DescriptorSetLayout& descriptorSetLayout : descriptorSetLayouts)
        vkDescriptorSetLayouts.emplace_back(descriptorSetLayout.get());

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool.get();
    allocInfo.descriptorSetCount = static_cast<uint32_t>(vkDescriptorSetLayouts.size());
    allocInfo.pSetLayouts = vkDescriptorSetLayouts.data();

    vkCheck(vkAllocateDescriptorSets(device.get(), &allocInfo, vkDescriptorSets.data()), "Failed to allocate descriptor sets");
}

void DescriptorSet::linkBuffer(LogicalDevice device, Buffer buffer) {
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = buffer.get();
    bufferInfo.offset = 0;  //! should be a parameter
    bufferInfo.range = buffer.getSize();

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptorSet;
    descriptorWrite.dstBinding = 0;         //! should be a parameter
    descriptorWrite.dstArrayElement = 0;    //! should be a parameter
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; //! can be other type of buffer -> should be a parameter
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(device.get(), 1, &descriptorWrite, 0, nullptr);
}

void DescriptorSet::bind(CommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, VkPipelineBindPoint bindPoint) {
    vkCmdBindDescriptorSets(commandBuffer.get(), bindPoint, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
}
