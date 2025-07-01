#include "shader.hpp"

#include "./logicalDevice.hpp"
#include "./utils.hpp"

void Shader::init(LogicalDevice device, std::string filename) {
    std::vector<char> code = Shader::read(filename);

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<uint32_t*>(code.data());

    vkCheck(vkCreateShaderModule(device.get(), &createInfo, nullptr, &shaderModule), "Failed to create shader module");
}

void Shader::destroy(LogicalDevice device) {
    vkDestroyShaderModule(device.get(), shaderModule, nullptr);
}

VkPipelineShaderStageCreateInfo Shader::getShaderStageCreateInfo(VkShaderStageFlagBits stage) {
    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = stage;
    shaderStageInfo.module = shaderModule;
    shaderStageInfo.pName = "main"; // TODO: make the entrypoint customisable

    return shaderStageInfo;
}


std::vector<char> Shader::read(std::string filename) {
    std::ifstream file(filename.c_str(), std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        char buf[MAX_MSG_SIZE];
        snprintf(buf, sizeof(buf), "Failed to open file [%s%s%s]", MSG_RED_B, filename.c_str(), MSG_RESET);
        throw std::runtime_error(buf);
    }

    size_t fileSize = (size_t)file.tellg(); // Get the size by reading the position from the end of the file
    std::vector<char> buffer(fileSize);

    file.seekg(0);  // Return to the start of the file
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

std::string Shader::compile(std::string filename, VkShaderStageFlagBits stage, bool alwaysCompile) {
    // Check if the file exist
    if (!std::filesystem::exists(filename.c_str())) {
        char buf[MAX_MSG_SIZE];
        snprintf(buf, sizeof(buf), "The shader file [%s%s%s] does not exist", MSG_RED_B, filename.c_str(), MSG_RESET);
        throw std::runtime_error(buf);
    }

    // Check if the .glsl extension is there
    size_t idx = filename.find(".glsl");
    if (idx == std::string::npos) {
        char buf[MAX_MSG_SIZE];
        snprintf(buf, sizeof(buf), "The shader file [%s%s%s] does not have the .glsl file extension", MSG_RED_B, filename.c_str(), MSG_RESET);
        throw std::runtime_error(buf);
    }

    std::string compiledFilename = filename.substr(0, idx) + ".spv";
    // File already exist so we don't need to recompile (because alwaysCompile=false)
    if (!alwaysCompile && std::filesystem::exists(compiledFilename))
        return compiledFilename;

    // Choose the shader stage based on the ShaderStageFlag
    std::string shaderStage;
    switch (stage) {
        case VK_SHADER_STAGE_VERTEX_BIT: shaderStage = "vertex"; break;
        case VK_SHADER_STAGE_FRAGMENT_BIT: shaderStage = "fragment"; break;
        default:
            char buf[MAX_MSG_SIZE];
            snprintf(buf, sizeof(buf), "The shader file [%s%s%s] does not have a supported stage for the compilation", MSG_RED_B, filename.c_str(), MSG_RESET);
            throw std::runtime_error(buf);
    }
    
    //! Might be UNIX specific
    char cmd[MAX_MSG_SIZE];
    snprintf(cmd, sizeof(cmd), "$VULKAN_SDK/bin/glslc -fshader-stage=%s %s -o %s", shaderStage.c_str(), filename.c_str(), compiledFilename.c_str());

    if (system(cmd) != 0) {
        char buf[MAX_MSG_SIZE];
        snprintf(buf, sizeof(buf), "Failed to compile the shader [%s%s%s]", MSG_RED_B, filename.c_str(), MSG_RESET);
        throw std::runtime_error(buf);
    }

    // std::cout << "output : " << compiledFilename << std::endl;

    return compiledFilename;
}
