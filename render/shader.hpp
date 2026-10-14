#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "../base/unique_handle.hpp"

namespace VkSmol {

class LogicalDevice;
class ShaderFileSystem;

using ShaderDefines = std::vector<std::pair<std::string, std::string>>;

struct ShaderLocation {
    uint32_t set = 0;
    uint32_t binding = 0;

    ShaderLocation() = default;
    ShaderLocation(uint32_t binding): binding(binding) {}
    ShaderLocation(uint32_t set, uint32_t binding): set(set), binding(binding) {}
};

struct ShaderParameter {
    std::string      name;
    ShaderLocation   location;
    VkDescriptorType descriptorType;
    bool             writable;
    bool             used;
};

struct ShaderDependency {
    std::string path;
    size_t      hash;
};

struct ShaderCompilation {
    std::vector<uint32_t>         spirv;
    VkShaderStageFlagBits         stage;
    std::vector<ShaderParameter>  parameters;
    uint32_t                      pushConstantSize = 0;
    std::vector<ShaderDependency> dependencies;
};

class Shader: UniqueHandle<VkShaderModule, VK_NULL_HANDLE> {
public:
    void init(const LogicalDevice& device, VkShaderStageFlagBits stage, const std::vector<uint32_t>& spirv);
    void destroy(const LogicalDevice& device);
    VkPipelineShaderStageCreateInfo getShaderStageCreateInfo() const;

    static ShaderCompilation compile(const std::string& path, const ShaderDefines& defines, ShaderFileSystem& fileSystem);
    static bool isUpToDate(const std::vector<ShaderDependency>& dependencies, ShaderFileSystem& fileSystem);

private:
    class SlangFileSystem;

    static std::vector<uint32_t> optimize(const uint32_t* words, size_t wordCount, const std::string& name);

    VkShaderStageFlagBits stage;
};

}
