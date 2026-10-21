#include "device_features.hpp"

#include <algorithm>

namespace VkSmol {

DeviceFeatureChain::DeviceFeatureChain(const EngineFeatures& requested, bool presentation) {
    extensions = {VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME, VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME};
#ifdef __APPLE__
    extensions.emplace_back("VK_KHR_portability_subset");
#endif
    if (presentation) extensions.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    if (requested.bufferFloatAtomicAdd || requested.sharedFloatAtomicAdd || requested.imageFloatAtomicAdd)
        extensions.emplace_back(VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME);

    features.features.samplerAnisotropy = requested.samplerAnisotropy;
    features.features.fragmentStoresAndAtomics = requested.fragmentStoresAndAtomics;
    dynamicRendering.dynamicRendering = VK_TRUE;
    synchronization2.synchronization2 = VK_TRUE;
    shaderDrawParameters.shaderDrawParameters = VK_TRUE;
    atomicFloat.shaderBufferFloat32AtomicAdd = requested.bufferFloatAtomicAdd;
    atomicFloat.shaderSharedFloat32AtomicAdd = requested.sharedFloatAtomicAdd;
    atomicFloat.shaderImageFloat32AtomicAdd = requested.imageFloatAtomicAdd;
    link();
}

DeviceFeatureChain::DeviceFeatureChain(VkPhysicalDevice device) {
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> properties(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, properties.data());
    for (const VkExtensionProperties& property : properties) extensions.emplace_back(property.extensionName);

    link();
    vkGetPhysicalDeviceFeatures2(device, &features);
}

std::vector<const char*> DeviceFeatureChain::getExtensionNames() const {
    std::vector<const char*> names;
    for (const std::string& extension : extensions) names.push_back(extension.c_str());
    return names;
}

std::vector<std::string> DeviceFeatureChain::findMissing(const DeviceFeatureChain& supported) const {
    std::vector<std::string> missing;
    for (const std::string& extension : extensions)
        if (!supported.hasExtension(extension.c_str())) missing.push_back(extension);

    const auto check = [&](const char* name, VkBool32 requested, VkBool32 available) {
        if (requested && !available) missing.emplace_back(name);
    };
    check("samplerAnisotropy", features.features.samplerAnisotropy, supported.features.features.samplerAnisotropy);
    check(
        "fragmentStoresAndAtomics", features.features.fragmentStoresAndAtomics,
        supported.features.features.fragmentStoresAndAtomics
    );
    check("dynamicRendering", dynamicRendering.dynamicRendering, supported.dynamicRendering.dynamicRendering);
    check("synchronization2", synchronization2.synchronization2, supported.synchronization2.synchronization2);
    check(
        "shaderDrawParameters", shaderDrawParameters.shaderDrawParameters,
        supported.shaderDrawParameters.shaderDrawParameters
    );
    check(
        "shaderBufferFloat32AtomicAdd", atomicFloat.shaderBufferFloat32AtomicAdd,
        supported.atomicFloat.shaderBufferFloat32AtomicAdd
    );
    check(
        "shaderSharedFloat32AtomicAdd", atomicFloat.shaderSharedFloat32AtomicAdd,
        supported.atomicFloat.shaderSharedFloat32AtomicAdd
    );
    check(
        "shaderImageFloat32AtomicAdd", atomicFloat.shaderImageFloat32AtomicAdd,
        supported.atomicFloat.shaderImageFloat32AtomicAdd
    );
    return missing;
}

bool DeviceFeatureChain::hasExtension(const char* name) const {
    return std::ranges::find(extensions, std::string(name)) != extensions.end();
}

void DeviceFeatureChain::link() {
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    dynamicRendering.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    synchronization2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
    shaderDrawParameters.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES;
    atomicFloat.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT;
    features.pNext = &dynamicRendering;
    dynamicRendering.pNext = &synchronization2;
    synchronization2.pNext = &shaderDrawParameters;
    shaderDrawParameters.pNext = hasExtension(VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME) ? &atomicFloat : nullptr;
    atomicFloat.pNext = nullptr;
}

} // namespace VkSmol
