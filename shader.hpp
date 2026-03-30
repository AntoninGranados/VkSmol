#pragma once

#include <vulkan/vulkan.h>

#include <vector>
#include <string>

#include "base/unique_handle.hpp"

class LogicalDevice;

class Shader: UniqueHandle<VkShaderModule, VK_NULL_HANDLE> {
public:
    // The shader needs to be precompiled and the `filename` is the SPIR-V file
    void init(const LogicalDevice& device, VkShaderStageFlagBits stage, std::string filepath);
    void destroy(const LogicalDevice& device);
    VkPipelineShaderStageCreateInfo getShaderStageCreateInfo(std::string entryPoint = "main") const;

    // If `alwaysCompile` is set to `false` (`true` value by default), the compilation will not be done if the SPIR-V file already exists
    // Returns the name of the compiled file
    static std::string compile(std::string filename, VkShaderStageFlagBits stage, bool alwaysCompile=true);

    
private:
    VkShaderStageFlagBits stage;

    // Read a SPIR-V file into a vector representation for easy access during ShaderModule creation
    static std::vector<char> read(std::string filename);
};
