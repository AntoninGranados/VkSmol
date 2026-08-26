#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <string>

#include "../../base/unique_handle.hpp"

class LogicalDevice;

class PipelineCache: public UniqueHandle<VkPipelineCache, VK_NULL_HANDLE> {
public:
    void init(const LogicalDevice& device, std::string filePath = {});
    void destroy(const LogicalDevice& device);

private:
    std::string filePath;
};
