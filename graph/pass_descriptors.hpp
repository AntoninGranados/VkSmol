#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "../render/descriptor/descriptor_set_group.hpp"
#include "../render/descriptor/descriptor_set_layout.hpp"
#include "../render/descriptor/descriptor_writer.hpp"
#include "compiled_resource.hpp"

namespace VkSmol {

class CommandBuffer;
class Device;
class LogicalDevice;
class ResourceRegistry;

class PassDescriptors {
public:
    PassDescriptors(Device& device, ResourceRegistry& resources);

    void init(const CompiledRenderGraph& graph);
    void destroy();

    std::vector<const DescriptorSetLayout*> getLayouts(uint32_t passId) const;
    void bind(const CommandBuffer& commandBuffer, const CompiledPass& pass, uint32_t frame, VkPipelineLayout layout);

private:
    Device& device;
    const LogicalDevice& logicalDevice;
    ResourceRegistry& resources;

    std::unordered_map<uint32_t, std::vector<DescriptorSetLayout>> layouts;
    std::unordered_map<uint32_t, std::vector<DescriptorSetGroup>> groups;
    std::unordered_map<uint32_t, std::vector<uint64_t>> written;

    DescriptorSetGroup createGroup(DescriptorSetLayout& layout, std::string name);
    void destroyGroup(DescriptorSetGroup& group);
    std::vector<uint64_t> resolve(const CompiledPass& pass);
    void update(const CompiledPass& pass, uint32_t frame);
    void write(DescriptorSetGroup& group, uint32_t frame, const DescriptorWriter& writer);
};

} // namespace VkSmol
