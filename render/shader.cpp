#include "shader.hpp"

#include <shaderc/shaderc.hpp>

#include <format>

#include "../device/logical_device.hpp"
#include "../utils.hpp"

void Shader::init(const LogicalDevice& device, VkShaderStageFlagBits stage_, const std::vector<uint32_t>& spirv) {
    stage = stage_;

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = spirv.size() * sizeof(uint32_t);
    createInfo.pCode = spirv.data();

    vkCheck(vkCreateShaderModule(device.get(), &createInfo, nullptr, &rawHandle()), "Failed to create shader module");
}

void Shader::destroy(const LogicalDevice& device) {
    vkDestroyShaderModule(device.get(), rawHandle(), nullptr);
}

VkPipelineShaderStageCreateInfo Shader::getShaderStageCreateInfo() const {
    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = stage;
    shaderStageInfo.module = rawHandle();
    shaderStageInfo.pName = "main";

    return shaderStageInfo;
}

std::vector<uint32_t> Shader::compile(const std::string& code, VkShaderStageFlagBits stage, const std::string& name) {
    shaderc_shader_kind kind;
    switch (stage) {
        case VK_SHADER_STAGE_VERTEX_BIT:   kind = shaderc_glsl_vertex_shader; break;
        case VK_SHADER_STAGE_FRAGMENT_BIT: kind = shaderc_glsl_fragment_shader; break;
        case VK_SHADER_STAGE_COMPUTE_BIT:  kind = shaderc_glsl_compute_shader; break;
        default:
            throw std::runtime_error(std::format("The shader [{}{}{}] does not have a supported stage for the compilation", MSG_RED_B, name, MSG_RESET));
    }

    shaderc::Compiler compiler;
    shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(code, kind, name.c_str());
    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        throw std::runtime_error(std::format("Failed to compile the shader [{}{}{}]: {}", MSG_RED_B, name, MSG_RESET, result.GetErrorMessage()));
    }

    return { result.cbegin(), result.cend() };
}
