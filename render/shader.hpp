#pragma once

#include <vulkan/vulkan.h>

#include <compare>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "../base/unique_handle.hpp"

namespace VkSmol {

class LogicalDevice;
class ShaderFileSystem;

using ShaderDefines = std::vector<std::pair<std::string, std::string>>;

struct ShaderConformance {
    std::string type;
    std::string interfaceType;
    int id;
    std::string modulePath;

    auto operator<=>(const ShaderConformance&) const = default;
};

using ShaderConformances = std::vector<ShaderConformance>;

enum class ShaderFieldType : uint16_t {
    Bool,
    Int,
    UInt,
    Float,
    Struct,
};

struct ShaderAttribute {
    std::string name;
    std::vector<float> arguments;
};

struct ShaderField {
    std::string name;
    ShaderFieldType type;
    uint32_t componentCount = 0;
    std::vector<uint32_t> defaultValue;
    std::vector<ShaderAttribute> attributes;
    std::vector<ShaderField> fields;
};

struct ShaderStruct {
    std::string name;
    std::vector<ShaderAttribute> attributes;
    std::vector<ShaderField> fields;
};

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
    VkImageViewType viewType;
    bool             writable;
    bool             used;
};

struct ShaderDependency {
    std::string path;
    size_t      hash;
};

struct ShaderStageCompilation {
    std::string path;
    VkShaderStageFlagBits stage;
    std::vector<uint32_t> spirv;
};

struct ShaderCompilation {
    std::vector<ShaderStageCompilation> stages;
    std::vector<ShaderParameter>  parameters;
    uint32_t                      pushConstantSize = 0;
    VkExtent3D                    threadGroupSize = { 1, 1, 1 };
    std::vector<ShaderDependency> dependencies;
};

class Shader: UniqueHandle<VkShaderModule, VK_NULL_HANDLE> {
public:
    void init(const LogicalDevice& device, VkShaderStageFlagBits stage, const std::vector<uint32_t>& spirv);
    void destroy(const LogicalDevice& device);
    VkPipelineShaderStageCreateInfo getShaderStageCreateInfo() const;

    static ShaderCompilation compile(
        const std::vector<std::pair<VkShaderStageFlagBits, std::string>>& stages, const ShaderDefines& defines,
        const ShaderConformances& conformances, ShaderFileSystem& fileSystem
    );
    static std::vector<ShaderStruct>
    reflect(const std::string& path, const std::string& interfaceName, ShaderFileSystem& fileSystem);
    static bool isUpToDate(const std::vector<ShaderDependency>& dependencies, ShaderFileSystem& fileSystem);

private:
    class SlangFileSystem;
    class SlangContext;

    static std::vector<uint32_t> optimize(const uint32_t* words, size_t wordCount, const std::string& name);

    VkShaderStageFlagBits stage;
};

}
