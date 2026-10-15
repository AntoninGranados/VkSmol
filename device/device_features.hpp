#pragma once

#include <vulkan/vulkan.h>

#include <string>
#include <vector>

#include "engine_features.hpp"

namespace VkSmol {

class DeviceFeatureChain {
public:
    DeviceFeatureChain(const EngineFeatures& requested, bool presentation);
    explicit DeviceFeatureChain(VkPhysicalDevice device);

    DeviceFeatureChain(const DeviceFeatureChain&) = delete;
    DeviceFeatureChain& operator=(const DeviceFeatureChain&) = delete;
    DeviceFeatureChain(DeviceFeatureChain&&) = delete;
    DeviceFeatureChain& operator=(DeviceFeatureChain&&) = delete;

    const VkPhysicalDeviceFeatures2& getFeatures() const { return features; }
    std::vector<const char*> getExtensionNames() const;
    std::vector<std::string> findMissing(const DeviceFeatureChain& supported) const;

private:
    VkPhysicalDeviceFeatures2 features{};
    VkPhysicalDeviceDynamicRenderingFeatures dynamicRendering{};
    VkPhysicalDeviceSynchronization2Features synchronization2{};
    VkPhysicalDeviceShaderDrawParametersFeatures shaderDrawParameters{};
    VkPhysicalDeviceShaderAtomicFloatFeaturesEXT atomicFloat{};
    std::vector<std::string> extensions;

    bool hasExtension(const char* name) const;
    void link();
};

} // namespace VkSmol
