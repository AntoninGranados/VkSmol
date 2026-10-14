#include "device.hpp"
#include "engine_version.hpp"

#include <algorithm>
#include <cstring>
#include <format>
#include <stdexcept>

#include "../image/image_barrier.hpp"
#include "../present/swapchain.hpp"
#include "../present/window.hpp"

namespace VkSmol {

const std::vector<const char*> instanceExtensions = {
#ifdef __APPLE__
    VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
#ifndef NDEBUG
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
};

const std::vector<const char*> deviceExtensions = {
    // VK_KHR_SWAPCHAIN_EXTENSION_NAME, // added automatically when not in headless mode
    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
    VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
#ifdef __APPLE__
    "VK_KHR_portability_subset",
#endif
};

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation",
};

Device::Device(const std::string& appName, uint32_t appVersion, std::optional<const Window&> window): headless(!window.has_value()) {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = appName.c_str();
    appInfo.applicationVersion = appVersion;
    appInfo.pEngineName = "VkSmol";
    appInfo.engineVersion = VK_SMOL_VERSION;
    appInfo.apiVersion = VK_API_VERSION_1_4;

    instance.init(appInfo, getRequiredInstanceExtensions(window), validationLayers);
    debugMessenger.init(instance);
    if (window) surface.init(instance, *window);

    physicalDevice.init(instance, [this](const PhysicalDevice& device) { return scoreDevice(device); });
    queueFamilies = findQueueFamilies(physicalDevice, headless ? nullptr : &surface);

    logicalDevice.init(physicalDevice, queueFamilies.getIndices(headless), getRequiredDeviceExtensions(), validationLayers);

    graphicsQueue.init(logicalDevice, queueFamilies.graphicsFamily.value());
    transferQueue.init(logicalDevice, queueFamilies.transferFamily.value());
    if (!headless) presentQueue.init(logicalDevice, queueFamilies.presentFamily.value());

    graphicsCommandPool.init(logicalDevice, queueFamilies.graphicsFamily.value());
    transferCommandPool.init(logicalDevice, queueFamilies.transferFamily.value(), VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
    immediateCommandPool.init(logicalDevice, queueFamilies.graphicsFamily.value(), VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
}

Device::~Device() {
    waitIdle();

    graphicsCommandPool.destroy(logicalDevice);
    transferCommandPool.destroy(logicalDevice);
    immediateCommandPool.destroy(logicalDevice);

    logicalDevice.destroy();
    if (!headless) surface.destroy(instance);
    debugMessenger.destroy(instance);
    instance.destroy();
}

// ——— Shaders ————————————————————————————————————————————————————————————————

const Device::CompiledShader& Device::compileShader(const std::string& path, const ShaderDefines& defines) {
    CompiledShader& compiled = compiledShaders[{path, defines}];
    if (compiled.checked)
        return compiled;

    compiled.error.clear();
    if (compiled.compilation.spirv.empty() || !Shader::isUpToDate(compiled.compilation.dependencies, *shaderFileSystem)) {
        try {
            compiled.compilation = Shader::compile(path, defines, *shaderFileSystem);
        } catch (const std::exception& e) {
            if (compiled.compilation.spirv.empty())
                throw;
            compiled.error = e.what();
        }
    }
    compiled.checked = true;
    return compiled;
}

void Device::markShadersStale() {
    for (auto& [key, compiled] : compiledShaders)
        compiled.checked = false;
}

ShaderLocation Device::getShaderLocation(const std::string& shaderPath, const std::string& parameterName, const ShaderDefines& defines) {
    for (const ShaderParameter& parameter : compileShader(shaderPath, defines).compilation.parameters)
        if (parameter.name == parameterName) return parameter.location;

    throw std::runtime_error(std::format("The shader [{}{}{}] has no parameter named [{}]", MSG_RED_B, shaderPath, MSG_RESET, parameterName));
}

// ——— Command buffers ————————————————————————————————————————————————————————

CommandBuffer Device::allocCommandBuffer() {
    CommandBuffer cb;
    cb.alloc(logicalDevice, graphicsCommandPool);
    return cb;
}

void Device::freeCommandBuffer(CommandBuffer& cb) {
    cb.free(logicalDevice, graphicsCommandPool);
}

void Device::immediateSubmit(const std::function<void(const CommandBuffer&)>& record) {
    CommandBuffer commandBuffer = CommandBuffer::beginSingleTimeCommands(logicalDevice, immediateCommandPool);
    record(commandBuffer);
    CommandBuffer::endSingleTimeCommands(logicalDevice, immediateCommandPool, graphicsQueue, commandBuffer);
}

// ——— Buffers ————————————————————————————————————————————————————————————————

Buffer Device::createBuffer(VkBufferUsageFlags usage, size_t size, BufferAllocFlags flags, void* data, std::string name) {
    Buffer buf;
    buf.setResourceName(name.empty() ? "Buffer" : std::move(name));

    if (flags & BufferAllocMapped) {
        buf.init(logicalDevice, usage, size, true);
        buf.alloc(logicalDevice, physicalDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (data) buf.fill(logicalDevice, data);
    } else if (data) {
        buf.initFilled(logicalDevice, physicalDevice, usage, transferCommandPool, transferQueue, size, data);
    } else {
        buf.init(logicalDevice, usage, size);
        buf.alloc(logicalDevice, physicalDevice, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }

    return buf;
}

void Device::destroyBuffer(Buffer& buffer) {
    if (!buffer.get()) return;
    auto retired = std::make_shared<Buffer>(std::move(buffer));
    retire([retired](const LogicalDevice& d) { retired->free(d); retired->destroy(d); });
    buffer = Buffer{};
}

// ——— Images —————————————————————————————————————————————————————————————————

Image Device::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, std::string name) {
    Image img;
    img.setResourceName(name.empty() ? "Image" : std::move(name));
    img.init(logicalDevice, width, height, format, usage);
    img.alloc(logicalDevice, physicalDevice, properties);
    return img;
}

Image Device::createImageFromPath(std::string path, std::string name) {
    Image img;
    img.setResourceName(name.empty() ? "Image" : std::move(name));
    img.initFromPath(logicalDevice, physicalDevice, transferCommandPool, transferQueue, path);
    return img;
}

Image Device::createImageFromData(const void* data, size_t size, VkFormat format, uint32_t width, uint32_t height, std::string name) {
    Buffer staging;
    staging.init(logicalDevice, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, size, true);
    staging.alloc(logicalDevice, physicalDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    staging.fill(logicalDevice, const_cast<void*>(data));

    Image img;
    img.setResourceName(name.empty() ? "Image" : std::move(name));
    img.init(logicalDevice, width, height, format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    img.alloc(logicalDevice, physicalDevice, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    CommandBuffer cmd = CommandBuffer::beginSingleTimeCommands(logicalDevice, transferCommandPool);
    img.fillFromBuffer(logicalDevice, cmd, staging);
    CommandBuffer::endSingleTimeCommands(logicalDevice, transferCommandPool, transferQueue, cmd);

    staging.free(logicalDevice);
    staging.destroy(logicalDevice);

    return img;
}

Image Device::createEmptyImage(VkFormat format, uint32_t width, uint32_t height, std::string name) {
    Image img;
    img.setResourceName(name.empty() ? "Image" : std::move(name));
    img.init(logicalDevice, width, height, format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    img.alloc(logicalDevice, physicalDevice, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    return img;
}

void Device::fillImage(Image& image, const void* data, size_t size) {
    Buffer staging;
    staging.init(logicalDevice, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, size, true);
    staging.alloc(logicalDevice, physicalDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    staging.fill(logicalDevice, const_cast<void*>(data));

    CommandBuffer cmd = CommandBuffer::beginSingleTimeCommands(logicalDevice, transferCommandPool);
    image.fillFromBuffer(logicalDevice, cmd, staging);
    CommandBuffer::endSingleTimeCommands(logicalDevice, transferCommandPool, transferQueue, cmd);

    staging.free(logicalDevice);
    staging.destroy(logicalDevice);
}

void Device::readImage(Image& image, void* out, size_t size) {
    Buffer readback = createBuffer(VK_BUFFER_USAGE_TRANSFER_DST_BIT, size, BufferAllocMapped, nullptr, "ImageReadback");

    CommandBuffer commandBuffer = CommandBuffer::beginSingleTimeCommands(logicalDevice, immediateCommandPool);
    emitMemoryBarrier(commandBuffer,
        VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
    image.copyToBuffer(commandBuffer, readback);
    emitMemoryBarrier(commandBuffer,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT);
    CommandBuffer::endSingleTimeCommands(logicalDevice, immediateCommandPool, graphicsQueue, commandBuffer);

    readback.read(logicalDevice, out, size);
    readback.free(logicalDevice);
    readback.destroy(logicalDevice);
}

void Device::copyImage(Image& src, Image& dst) {
    CommandBuffer cmd = CommandBuffer::beginSingleTimeCommands(logicalDevice, immediateCommandPool);

    ImageBarrier barrier;
    barrier.call(
        logicalDevice, cmd, dst.get(),
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_ACCESS_2_NONE, VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT
    );

    src.copyToImage(cmd, dst);

    barrier.call(
        logicalDevice, cmd, dst.get(),
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT
    );

    CommandBuffer::endSingleTimeCommands(logicalDevice, immediateCommandPool, graphicsQueue, cmd);
}

void Device::destroyImage(Image& image) {
    if (!image.get()) return;
    auto retired = std::make_shared<Image>(std::move(image));
    retire([retired](const LogicalDevice& d) { retired->free(d); retired->destroy(d); });
    image = Image{};
}

ImageView Device::createImageView(Image& image, std::string name) {
    if (name.empty() && !image.getResourceName().empty())
        name = image.getResourceName() + "View";
    ImageView view;
    view.setResourceName(name.empty() ? "ImageView" : std::move(name));
    view.init(logicalDevice, image);
    return view;
}

void Device::destroyImageView(ImageView& view) {
    if (!view.get()) return;
    auto retired = std::make_shared<ImageView>(std::move(view));
    retire([retired](const LogicalDevice& d) { retired->destroy(d); });
    view = ImageView{};
}

Sampler Device::createSampler(std::string name) {
    Sampler smp;
    smp.setResourceName(name.empty() ? "Sampler" : std::move(name));
    smp.init(logicalDevice, physicalDevice);
    return smp;
}

void Device::destroySampler(Sampler& sampler) {
    if (!sampler.get()) return;
    auto retired = std::make_shared<Sampler>(std::move(sampler));
    retire([retired](const LogicalDevice& d) { retired->destroy(d); });
    sampler = Sampler{};
}

// ——— Synchronization ————————————————————————————————————————————————————————

void Device::emitMemoryBarrier(
    const CommandBuffer& commandBuffer,
    VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess,
    VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess
) const {
    const VkMemoryBarrier2 barrier{
        .sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask  = srcStage,
        .srcAccessMask = srcAccess,
        .dstStageMask  = dstStage,
        .dstAccessMask = dstAccess,
    };
    const VkDependencyInfo dependency{
        .sType              = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = 1,
        .pMemoryBarriers    = &barrier,
    };
    logicalDevice.functions().cmdPipelineBarrier2(commandBuffer.get(), &dependency);
}

void Device::retire(std::function<void(const LogicalDevice&)> payload) {
    retirementQueue[currentRetirementFrame].push_back(std::move(payload));
}

void Device::collectRetired(uint32_t frame) {
    for (auto& payload : retirementQueue[frame]) payload(logicalDevice);
    retirementQueue[frame].clear();
    currentRetirementFrame = frame;
}

void Device::flushRetired() {
    for (auto& queue : retirementQueue) {
        for (auto& payload : queue) payload(logicalDevice);
        queue.clear();
    }
}

void Device::waitIdle() {
    logicalDevice.waitIdle();
    flushRetired();
}

// ——— Device selection ———————————————————————————————————————————————————————

std::vector<const char*> Device::getRequiredInstanceExtensions(std::optional<const Window&> window) const {
    std::vector<const char*> requiredExtensions;

    if (window)
        for (const char* ext : window->getRequiredInstanceExtensions())
            requiredExtensions.emplace_back(ext);

    requiredExtensions.insert(requiredExtensions.end(), instanceExtensions.begin(), instanceExtensions.end());

    return requiredExtensions;
}

std::vector<const char*> Device::getRequiredDeviceExtensions() const {
    std::vector<const char*> requiredExtensions = deviceExtensions;
    if (!headless) requiredExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    return requiredExtensions;
}

int Device::scoreDevice(const PhysicalDevice& device) const {
    QueueFamilyIndices indices = findQueueFamilies(device, headless ? nullptr : &surface);
    if (!indices.isComplete(headless)) return 0;

    // Check if all extensions are supported by the GPU
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device.get(), nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> supportedExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device.get(), nullptr, &extensionCount, supportedExtensions.data());

    for (const char* extension : getRequiredDeviceExtensions()) {
        if (!std::any_of(
            supportedExtensions.begin(),
            supportedExtensions.end(),
            [&](VkExtensionProperties& properties) { return strcmp(extension, properties.extensionName) == 0; })
        )
            return 0;
    }

    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device.get(), &supportedFeatures);
    if (!supportedFeatures.samplerAnisotropy) return 0;

    if (!supportedFeatures.fragmentStoresAndAtomics) return 0;

    VkPhysicalDeviceSynchronization2Features synchronization2Features{};
    synchronization2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
    VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures{};
    dynamicRenderingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    dynamicRenderingFeatures.pNext = &synchronization2Features;
    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &dynamicRenderingFeatures;
    vkGetPhysicalDeviceFeatures2(device.get(), &features2);
    if (!dynamicRenderingFeatures.dynamicRendering || !synchronization2Features.synchronization2) return 0;

    // Check swap chain support
    if (!headless) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device, surface);
        if (swapChainSupport.formats.empty() || swapChainSupport.presentModes.empty()) return 0;
    }

    return 1;
}

}
