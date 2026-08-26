#include "shader.hpp"

#include <filesystem>
#include <format>
#include <fstream>
#include <system_error>

#include "../device/logical_device.hpp"
#include "../utils.hpp"

std::string Shader::spvOutputDir;
std::unordered_map<std::string, Shader::BuildRecord> Shader::buildRecords;

shaderc_include_result* Shader::Includer::GetInclude(const char* requestedSource, shaderc_include_type, const char* requestingSource, size_t) {
    auto* data = new Data();
    std::filesystem::path resolved = std::filesystem::path(requestingSource).parent_path() / requestedSource;

    std::ifstream file(resolved, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        data->content = std::format("Failed to open included file: {}", resolved.string());
    } else {
        size_t size = static_cast<size_t>(file.tellg());
        file.seekg(0);
        data->content.resize(size);
        file.read(data->content.data(), size);
        data->path = resolved.string();
        resolvedPaths.push_back(data->path);
    }

    auto* result = new shaderc_include_result();
    result->source_name = data->path.c_str();
    result->source_name_length = data->path.size();
    result->content = data->content.c_str();
    result->content_length = data->content.size();
    result->user_data = data;

    return result;
}

void Shader::Includer::ReleaseInclude(shaderc_include_result* data) {
    delete static_cast<Data*>(data->user_data);
    delete data;
}

// Public
void Shader::init(const LogicalDevice& device, VkShaderStageFlagBits stage_, std::string filepath, bool forceRecompile) {
    stage = stage_;

    bool isGLSL = filepath.find(".glsl") != std::string::npos;
    bool isSPIRV = filepath.find(".spv") != std::string::npos;

    std::string compiledPath;
    if (isGLSL) {
        CompileResult result = Shader::compile(filepath, stage, forceRecompile);
        compiledPath = result.spvPath;
        recompiled = result.recompiled;
    } else if (isSPIRV) {
        compiledPath = filepath;
        recompiled = true;
    } else {
        throw std::runtime_error("Failed to load shader, extension not recognized");
    }

    std::vector<char> code = Shader::read(compiledPath);

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<uint32_t*>(code.data());

    vkCheck(vkCreateShaderModule(device.get(), &createInfo, nullptr, &rawHandle()), "Failed to create shader module");
}

void Shader::destroy(const LogicalDevice& device) {
    vkDestroyShaderModule(device.get(), rawHandle(), nullptr);
}

VkPipelineShaderStageCreateInfo Shader::getShaderStageCreateInfo(std::string entryPoint) const {
    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = stage;
    shaderStageInfo.module = rawHandle();
    shaderStageInfo.pName = entryPoint.c_str();

    return shaderStageInfo;
}

void Shader::setSpvOutputDir(const std::string& dir) {
    spvOutputDir = dir;
}

Shader::CompileResult Shader::compile(std::string filepath, VkShaderStageFlagBits stage, bool forceRecompile) {
    // Check if the file exist
    if (!std::filesystem::exists(filepath.c_str())) {
        throw std::runtime_error(std::format("The shader file [{}{}{}] does not exist", MSG_RED_B, filepath.c_str(), MSG_RESET));
    }

    // Check if the .glsl extension is there
    size_t idx = filepath.find(".glsl");
    if (idx == std::string::npos) {
        throw std::runtime_error(std::format("The shader file [{}{}{}] does not have the .glsl file extension", MSG_RED_B, filepath.c_str(), MSG_RESET));
    }

    std::string compiledfilepath;
    if (!spvOutputDir.empty()) {
        std::filesystem::path out = std::filesystem::path(spvOutputDir) / (filepath.substr(0, idx) + ".spv");
        std::filesystem::create_directories(out.parent_path());
        compiledfilepath = out.string();
    } else {
        compiledfilepath = filepath.substr(0, idx) + ".spv";
    }

    std::string key = normalizeKey(filepath);
    auto recordIt = buildRecords.find(key);
    bool needsCompile = forceRecompile
        || recordIt == buildRecords.end()
        || !std::filesystem::exists(compiledfilepath)
        || isStale(recordIt->second);
    if (!needsCompile)
        return { compiledfilepath, false };

    shaderc_shader_kind kind;
    switch (stage) {
        case VK_SHADER_STAGE_VERTEX_BIT:   kind = shaderc_glsl_vertex_shader; break;
        case VK_SHADER_STAGE_FRAGMENT_BIT: kind = shaderc_glsl_fragment_shader; break;
        case VK_SHADER_STAGE_COMPUTE_BIT:  kind = shaderc_glsl_compute_shader; break;
        default:
            throw std::runtime_error(std::format("The shader file [{}{}{}] does not have a supported stage for the compilation", MSG_RED_B, filepath.c_str(), MSG_RESET));
    }

    std::vector<char> source = Shader::read(filepath);

    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    auto includer = std::make_unique<Includer>();
    Includer* includerPtr = includer.get();
    options.SetIncluder(std::move(includer));

    shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(source.data(), source.size(), kind, filepath.c_str(), options);
    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        throw std::runtime_error(std::format("Failed to compile the shader [{}{}{}]: {}", MSG_RED_B, filepath, MSG_RESET, result.GetErrorMessage()));
    }

    std::vector<uint32_t> spirv(result.cbegin(), result.cend());
    std::ofstream out(compiledfilepath, std::ios::binary);
    out.write(reinterpret_cast<const char*>(spirv.data()), spirv.size() * sizeof(uint32_t));

    buildRecords[key] = makeBuildRecord(filepath, includerPtr->getResolvedPaths());
    return { compiledfilepath, true };
}

bool Shader::isSourceStale(const std::string& filepath) {
    auto it = buildRecords.find(normalizeKey(filepath));
    return it == buildRecords.end() || isStale(it->second);
}

// Private helpers
std::string Shader::normalizeKey(const std::string& filepath) {
    return std::filesystem::absolute(filepath).lexically_normal().string();
}

bool Shader::isStale(const BuildRecord& record) {
    for (const auto& [path, recordedTime] : record.fileMtimes) {
        std::error_code ec;
        std::filesystem::file_time_type currentTime = std::filesystem::last_write_time(path, ec);
        if (ec || currentTime != recordedTime)
            return true;
    }
    return false;
}

Shader::BuildRecord Shader::makeBuildRecord(const std::string& mainFilepath, const std::vector<std::string>& includedPaths) {
    BuildRecord record;
    record.fileMtimes[normalizeKey(mainFilepath)] = std::filesystem::last_write_time(mainFilepath);
    for (const std::string& path : includedPaths)
        record.fileMtimes[normalizeKey(path)] = std::filesystem::last_write_time(path);
    return record;
}

std::vector<char> Shader::read(std::string filepath) {
    std::ifstream file(filepath.c_str(), std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error(std::format("Failed to open file [{}{}{}]", MSG_RED_B, filepath.c_str(), MSG_RESET));
    }

    size_t fileSize = static_cast<size_t>(file.tellg()); // Get the size by reading the position from the end of the file
    std::vector<char> buffer(fileSize);

    file.seekg(0);  // Return to the start of the file
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}
