#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>
#include <fstream>
#include <string>
#include <filesystem>

class LogicalDevice;

class Shader {
public:
    // TODO: compile the shader in this stage if a .glsl file is passed, if a .spv file is passed then continue (otherwise there is an error)
    // The shader needs to be precompiled and the `filename` is the SPIR-V file
    void init(LogicalDevice device, std::string filename);
    void destroy(LogicalDevice device);
    VkShaderModule get() { return shaderModule; };
    VkPipelineShaderStageCreateInfo getShaderStageCreateInfo(VkShaderStageFlagBits stage);

    // If `alwaysCompile` is set to `false` (`true` value by default), the compilation will not be done if the SPIR-V file already exists
    // Returns the name of the compiled file
    static std::string compile(std::string filename, VkShaderStageFlagBits stage, bool alwaysCompile=true);

    
private:
    VkShaderModule shaderModule;

    // Read a SPIR-V file into a vector representation for easy access during ShaderModule creation
    static std::vector<char> read(std::string filename);
};
