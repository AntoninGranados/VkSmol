#include "shader.hpp"

#include "./utils.hpp"

std::vector<char> readFile(const char* filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        char buf[MAX_MSG_SIZE];
        snprintf(buf, sizeof(buf), "Failed to open file [%s%s%s]", MSG_RED_B, filename, MSG_RESET);
        throw std::runtime_error(buf);
    }

    size_t fileSize = (size_t)file.tellg(); // Get the size by reading the position from the end of the file
    std::vector<char> buffer(fileSize);

    file.seekg(0);  // Return to the start of the file
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

const char* compileShader(const char* filenameChar, VkShaderStageFlagBits stage, bool alwaysCompile) {
    // Check if the file exist
    if (!std::filesystem::exists(filenameChar)) {
        char buf[MAX_MSG_SIZE];
        snprintf(buf, sizeof(buf), "The shader file [%s%s%s] does not exist", MSG_RED_B, filenameChar, MSG_RESET);
        throw std::runtime_error(buf);
    }

    // Check if the .glsl extension is there
    std::string filename = std::string(filenameChar);
    size_t idx = filename.find(".glsl");
    if (idx == std::string::npos) {
        char buf[MAX_MSG_SIZE];
        snprintf(buf, sizeof(buf), "The shader file [%s%s%s] does not have the .glsl file extension", MSG_RED_B, filenameChar, MSG_RESET);
        throw std::runtime_error(buf);
    }

    const char* compiledFilename = filename.replace(idx, 5, ".spv").c_str();

    // File already exist so we don't need to recompile (because alwaysCompile=false)
    if (!alwaysCompile && std::filesystem::exists(compiledFilename))
        return compiledFilename;

    // Choose the shader stage based on the ShaderStageFlag
    char* shaderStage = new char;
    switch (stage) {
        case VK_SHADER_STAGE_VERTEX_BIT: std::strcat(shaderStage, "vertex"); break;
        case VK_SHADER_STAGE_FRAGMENT_BIT: std::strcat(shaderStage, "fragment"); break;
        default:
            char buf[MAX_MSG_SIZE];
            snprintf(buf, sizeof(buf), "The shader file [%s%s%s] does not have a supported stage for the compilation", MSG_RED_B, filenameChar, MSG_RESET);
            throw std::runtime_error(buf);
    }
    
    //! Might be UNIX specific
    char cmd[MAX_MSG_SIZE];
    snprintf(cmd, sizeof(cmd), "$VULKAN_SDK/bin/glslc -fshader-stage=%s %s -o %s", shaderStage, filenameChar, compiledFilename);

    if (system(cmd) != 0) {
        char buf[MAX_MSG_SIZE];
        snprintf(buf, sizeof(buf), "Failed to compile the shader [%s%s%s]", MSG_RED_B, filenameChar, MSG_RESET);
        throw std::runtime_error(buf);
    }

    return compiledFilename;
}
