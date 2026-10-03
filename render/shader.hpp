#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>
#include <vector>

#include "../base/unique_handle.hpp"

class LogicalDevice;

class Shader: UniqueHandle<VkShaderModule, VK_NULL_HANDLE> {
public:
    void init(const LogicalDevice& device, VkShaderStageFlagBits stage, const std::vector<uint32_t>& spirv);
    void destroy(const LogicalDevice& device);
    VkPipelineShaderStageCreateInfo getShaderStageCreateInfo() const;

    static std::vector<uint32_t> compile(const std::string& code, VkShaderStageFlagBits stage, const std::string& name);

private:
    VkShaderStageFlagBits stage;
};
