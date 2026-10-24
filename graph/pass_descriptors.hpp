#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "../render/descriptor.hpp"
#include "builder_resource.hpp"

namespace VkSmol {

class CommandBuffer;
class Device;
class LogicalDevice;
class ResourceRegistry;

class PassDescriptors {
public:
    PassDescriptors(Device& device, ResourceRegistry& resources);

    void init(const GraphDeclaration& graph);
    void destroy();

    std::vector<const DescriptorSetLayout*> getLayouts(uint32_t passId) const;
    void bind(const CommandBuffer& commandBuffer, const PassInfo& pass, uint32_t frame, VkPipelineLayout layout);

private:
    Device& device;
    const LogicalDevice& logicalDevice;
    ResourceRegistry& resources;

    std::unordered_map<uint32_t, std::vector<DescriptorSetLayout>> layouts;
    std::unordered_map<uint32_t, std::vector<DescriptorSetGroup>> groups;
    std::unordered_map<uint32_t, std::vector<uint64_t>> written;

    DescriptorSetGroup createGroup(DescriptorSetLayout& layout);
    void destroyGroup(DescriptorSetGroup& group);
    std::vector<uint64_t> resolve(const PassInfo& pass);
    void update(const PassInfo& pass, uint32_t frame);
};

} // namespace VkSmol
