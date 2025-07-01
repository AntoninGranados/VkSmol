#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>
#include <fstream>
#include <string>
#include <filesystem>

// TODO: put this function in the ShaderModule class as a static member
std::vector<char> readFile(const char* filename);

// If `alwaysCompile` is set to `false` (`true` value by default), the compilation will not be done if the SPIR-V file already exists
// Returns the name of the compiled file
const char* compileShader(const char* filename, VkShaderStageFlagBits stage, bool alwaysCompile=true);
