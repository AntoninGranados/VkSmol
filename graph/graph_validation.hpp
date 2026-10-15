#pragma once

#include <string>
#include <vector>

#include "builder_resource.hpp"

namespace VkSmol {

class Device;
class ResourceRegistry;

void resolveBindings(
    std::vector<PassInfo>& passes, const std::vector<ComputePipelineInfo>& computePipelines,
    const std::vector<GraphicsPipelineInfo>& graphicsPipelines, Device& device, const ResourceRegistry& resources,
    std::vector<std::string>& errors
);
void checkImageAccesses(
    const std::vector<PassInfo>& passes, const ResourceRegistry& resources, std::vector<std::string>& errors
);
void checkAttachments(
    const std::vector<PassInfo>& passes, const std::vector<GraphicsPipelineInfo>& graphicsPipelines,
    const ResourceRegistry& resources, std::vector<std::string>& errors
);

} // namespace VkSmol
