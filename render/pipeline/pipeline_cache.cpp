#include "pipeline_cache.hpp"

#include <filesystem>
#include <fstream>
#include <vector>

#include "../../device/logical_device.hpp"
#include "../../utils.hpp"

void PipelineCache::init(const LogicalDevice& device, std::string filePath_) {
    filePath = std::move(filePath_);

    std::vector<char> initialData;
    if (!filePath.empty()) {
        try {
            std::ifstream file(filePath, std::ios::ate | std::ios::binary);
            if (file.is_open()) {
                size_t size = static_cast<size_t>(file.tellg());
                file.seekg(0);
                initialData.resize(size);
                file.read(initialData.data(), size);
            }
        } catch (...) {
            initialData.clear();
        }
    }

    VkPipelineCacheCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    createInfo.initialDataSize = initialData.size();
    createInfo.pInitialData = initialData.empty() ? nullptr : initialData.data();

    vkCheck(vkCreatePipelineCache(device.get(), &createInfo, nullptr, &rawHandle()), "Failed to create pipeline cache");
}

void PipelineCache::destroy(const LogicalDevice& device) {
    if (!filePath.empty()) {
        size_t size = 0;
        vkCheck(vkGetPipelineCacheData(device.get(), get(), &size, nullptr), "Failed to query pipeline cache data size");

        if (size > 0) {
            std::vector<char> data(size);
            vkCheck(vkGetPipelineCacheData(device.get(), get(), &size, data.data()), "Failed to retrieve pipeline cache data");

            std::filesystem::path out(filePath);
            if (out.has_parent_path()) std::filesystem::create_directories(out.parent_path());
            std::ofstream file(out, std::ios::binary);
            file.write(data.data(), static_cast<std::streamsize>(data.size()));
        }
    }

    vkDestroyPipelineCache(device.get(), rawHandle(), nullptr);
}
