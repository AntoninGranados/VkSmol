#pragma once

#include <vulkan/vulkan.h>

#include <shaderc/shaderc.hpp>

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "../base/unique_handle.hpp"

class LogicalDevice;

class Shader: UniqueHandle<VkShaderModule, VK_NULL_HANDLE> {
public:
    struct CompileResult {
        std::string spvPath;
        bool recompiled = false;
    };

    // The shader needs to be precompiled and the `filename` is the SPIR-V file
    void init(const LogicalDevice& device, VkShaderStageFlagBits stage, std::string filepath, bool forceRecompile = false);
    void destroy(const LogicalDevice& device);
    VkPipelineShaderStageCreateInfo getShaderStageCreateInfo(std::string entryPoint = "main") const;
    bool wasRecompiled() const { return recompiled; }

    // If `forceRecompile` is `false` (default) and the source hasn't changed since the last successful compile, compilation is skipped
    static CompileResult compile(std::string filename, VkShaderStageFlagBits stage, bool forceRecompile = false);
    static bool isSourceStale(const std::string& filepath);
    static void setSpvOutputDir(const std::string& dir);

private:
    class Includer : public shaderc::CompileOptions::IncluderInterface {
    public:
        shaderc_include_result* GetInclude(const char* requestedSource, shaderc_include_type type, const char* requestingSource, size_t includeDepth) override;
        void ReleaseInclude(shaderc_include_result* data) override;
        const std::vector<std::string>& getResolvedPaths() const { return resolvedPaths; }

    private:
        struct Data {
            std::string path;
            std::string content;
        };

        std::vector<std::string> resolvedPaths;
    };

    struct BuildRecord {
        std::unordered_map<std::string, std::filesystem::file_time_type> fileMtimes;
    };

    static std::string normalizeKey(const std::string& filepath);
    static bool isStale(const BuildRecord& record);
    static BuildRecord makeBuildRecord(const std::string& mainFilepath, const std::vector<std::string>& includedPaths);

    VkShaderStageFlagBits stage;
    bool recompiled = false;
    static std::string spvOutputDir;
    static std::unordered_map<std::string, BuildRecord> buildRecords;

    // Read a SPIR-V file into a vector representation for easy access during ShaderModule creation
    static std::vector<char> read(std::string filename);
};
