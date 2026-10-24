#include "pass_descriptors.hpp"

#include <algorithm>
#include <memory>
#include <utility>

#include "../command/command_buffer.hpp"
#include "../device/device.hpp"
#include "../resource/resource_registry.hpp"
#include "../utils.hpp"

namespace VkSmol {

PassDescriptors::PassDescriptors(Device& device, ResourceRegistry& resources)
    : device(device), logicalDevice(device.getLogicalDevice()), resources(resources) {}

void PassDescriptors::init(const GraphDeclaration& graph) {
    for (const PassInfo& pass : graph.passes) {
        if (pass.descriptorBindings.empty()) continue;
        const uint32_t passId = pass.id;

        std::vector<DescriptorBindingDecl> decls = pass.descriptorBindings;
        std::ranges::sort(decls, [](const DescriptorBindingDecl& a, const DescriptorBindingDecl& b) {
            return a.binding < b.binding;
        });

        uint32_t setCount = 0;
        for (const DescriptorBindingDecl& decl : decls) setCount = std::max(setCount, decl.set + 1);

        std::vector<DescriptorSetLayout>& passLayouts = layouts[passId];
        passLayouts = std::vector<DescriptorSetLayout>(setCount);
        for (const DescriptorBindingDecl& decl : decls)
            passLayouts[decl.set].addBinding(decl.binding, decl.stageFlags, decl.descriptorType);

        std::vector<DescriptorSetGroup>& passGroups = groups[passId];
        passGroups = std::vector<DescriptorSetGroup>(setCount);
        for (uint32_t set = 0; set < setCount; ++set) {
            passLayouts[set].init(logicalDevice);
            if (!passLayouts[set].getBindings().empty())
                passGroups[set] = createGroup(passLayouts[set]);
        }
    }
}

void PassDescriptors::destroy() {
    for (auto& [id, passGroups] : groups)
        for (DescriptorSetGroup& group : passGroups)
            if (group.isValid()) destroyGroup(group);
    groups.clear();
    for (auto& [id, passLayouts] : layouts)
        for (DescriptorSetLayout& layout : passLayouts) layout.destroy(logicalDevice);
    layouts.clear();
    written.clear();
}

std::vector<const DescriptorSetLayout*> PassDescriptors::getLayouts(uint32_t passId) const {
    const auto it = layouts.find(passId);
    if (it == layouts.end()) return {};
    std::vector<const DescriptorSetLayout*> result;
    for (const DescriptorSetLayout& layout : it->second) result.push_back(&layout);
    return result;
}

void PassDescriptors::bind(
    const CommandBuffer& commandBuffer, const PassInfo& pass, uint32_t frame, VkPipelineLayout layout
) {
    const auto it = groups.find(pass.id);
    if (it == groups.end()) return;

    std::vector<uint64_t> resolved = resolve(pass);
    std::vector<uint64_t>& current = written[pass.id * MAX_FRAME_IN_FLIGHT + frame];
    if (current != resolved) {
        update(pass, frame);
        current = std::move(resolved);
    }

    const VkPipelineBindPoint bindPoint =
        pass.type == PassType::Compute ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;
    for (uint32_t set = 0; set < it->second.size(); ++set)
        if (it->second[set].isValid()) it->second[set].at(frame).bind(commandBuffer, layout, bindPoint, set);
}

DescriptorSetGroup PassDescriptors::createGroup(DescriptorSetLayout& layout) {
    DescriptorPool pool;
    for (const auto& binding : layout.getBindings()) pool.addPoolSize(binding.descriptorType, MAX_FRAME_IN_FLIGHT);
    pool.init(logicalDevice, MAX_FRAME_IN_FLIGHT);

    std::vector<DescriptorSet> sets(MAX_FRAME_IN_FLIGHT);
    for (size_t i = 0; i < MAX_FRAME_IN_FLIGHT; ++i) sets[i].alloc(logicalDevice, pool, layout);
    return DescriptorSetGroup(std::move(pool), std::move(sets));
}

void PassDescriptors::destroyGroup(DescriptorSetGroup& group) {
    auto retired = std::make_shared<DescriptorSetGroup>(std::move(group));
    device.retire([retired](const LogicalDevice& d) { retired->destroy(d); });
    group = {};
}

std::vector<uint64_t> PassDescriptors::resolve(const PassInfo& pass) {
    std::vector<uint64_t> resolved;
    for (const DescriptorBindingDecl& decl : pass.descriptorBindings) {
        if (decl.isImage) {
            const ImageHandle image = {.id = decl.handleId};
            resolved.push_back(
                reinterpret_cast<uint64_t>(resources.getImageView({image, decl.subresources}, decl.viewType))
            );
            resolved.push_back(reinterpret_cast<uint64_t>(resources.getImage(image).ownedSampler.get()));
        } else {
            const BufferResource& buffer = resources.getBuffer({.id = decl.handleId});
            resolved.push_back(reinterpret_cast<uint64_t>(buffer.buffer));
            resolved.push_back(buffer.size);
        }
    }
    return resolved;
}

void PassDescriptors::update(const PassInfo& pass, uint32_t frame) {
    std::vector<DescriptorSetGroup>& passGroups = groups.at(pass.id);
    for (uint32_t set = 0; set < passGroups.size(); ++set) {
        if (!passGroups[set].isValid()) continue;

        DescriptorSet& descriptorSet = passGroups[set].at(frame);
        descriptorSet.clear();
        for (const DescriptorBindingDecl& decl : pass.descriptorBindings) {
            if (decl.set != set) continue;
            if (decl.isImage) {
                const ImageHandle image = {.id = decl.handleId};
                const VkImageView view = resources.getImageView({image, decl.subresources}, decl.viewType);
                if (decl.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                    descriptorSet.linkCombinedImageSampler(
                        view, resources.getImage(image).ownedSampler.get(), decl.binding
                    );
                else if (decl.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    descriptorSet.linkStorageImage(view, decl.binding);
            } else {
                const BufferResource& buffer = resources.getBuffer({.id = decl.handleId});
                if (decl.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                    descriptorSet.linkUniformBuffer(buffer.buffer, buffer.size, decl.binding);
                else if (decl.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
                    descriptorSet.linkStorageBuffer(buffer.buffer, buffer.size, decl.binding);
            }
        }
        descriptorSet.update(logicalDevice);
    }
}

} // namespace VkSmol
