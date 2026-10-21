#include "device.hpp"
#include "engine_version.hpp"

#include <algorithm>
#include <cstring>
#include <format>
#include <stdexcept>

#include "../present/swapchain.hpp"
#include "../present/window.hpp"

namespace VkSmol {

const std::vector<const char*> instanceExtensions = {
#ifdef __APPLE__
    VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
#endif
};

Device::Device(
    const std::string& appName, uint32_t appVersion, std::optional<const Window&> window, EngineFeatures features
)
    : headless(!window.has_value()), features(features) {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = appName.c_str();
    appInfo.applicationVersion = appVersion;
    appInfo.pEngineName = "VkSmol";
    appInfo.engineVersion = VK_SMOL_VERSION;
    appInfo.apiVersion = VK_API_VERSION_1_4;

    instance.init(appInfo, getRequiredInstanceExtensions(window), features.validation);
    if (features.validation) debugMessenger.init(instance);
    if (window) surface.init(instance, *window);

    const DeviceFeatureChain requested(features, !headless);
    selectPhysicalDevice(requested);
    queueFamilies = findQueueFamilies(physicalDevice, headless ? nullptr : &surface);

    logicalDevice.init(physicalDevice, queueFamilies.getIndices(headless), requested);

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

const Device::CompiledShader& Device::compileShader(
    const std::vector<std::string>& paths, const ShaderDefines& defines, const ShaderConformances& conformances
) {
    CompiledShader& compiled = compiledShaders[{paths, defines, conformances}];
    if (compiled.checked)
        return compiled;

    compiled.error.clear();
    if (compiled.compilation.stages.empty() ||
        !Shader::isUpToDate(compiled.compilation.dependencies, *shaderFileSystem)) {
        try {
            compiled.compilation = Shader::compile(paths, defines, conformances, *shaderFileSystem);
        } catch (const std::exception& e) {
            if (compiled.compilation.stages.empty()) throw;
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

Buffer Device::createBuffer(VkBufferUsageFlags usage, size_t size, BufferAllocFlags flags, std::string name) {
    Buffer buf;
    buf.setResourceName(name.empty() ? "Buffer" : std::move(name));

    if (flags & BufferAllocMapped) {
        buf.init(logicalDevice, usage, size, true);
        buf.alloc(logicalDevice, physicalDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
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

Image Device::createImage(
    uint32_t width, uint32_t height, uint32_t mipLevels, uint32_t arrayLayers, VkFormat format, VkImageUsageFlags usage,
    VkMemoryPropertyFlags properties, std::string name
) {
    Image img;
    img.setResourceName(name.empty() ? "Image" : std::move(name));
    img.init(logicalDevice, width, height, format, usage, mipLevels, arrayLayers);
    img.alloc(logicalDevice, physicalDevice, properties);
    return img;
}

void Device::destroyImage(Image& image) {
    if (!image.get()) return;
    auto retired = std::make_shared<Image>(std::move(image));
    retire([retired](const LogicalDevice& d) { retired->free(d); retired->destroy(d); });
    image = Image{};
}

ImageView
Device::createImageView(Image& image, VkImageViewType type, const VkImageSubresourceRange& range, std::string name) {
    if (name.empty() && !image.getResourceName().empty()) name = image.getResourceName() + "View";
    ImageView view;
    view.setResourceName(name.empty() ? "ImageView" : std::move(name));
    view.init(logicalDevice, image, type, range);
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
    smp.init(logicalDevice, physicalDevice, features.samplerAnisotropy);
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

void Device::selectPhysicalDevice(const DeviceFeatureChain& requested) {
    std::vector<std::string> rejections;
    try {
        physicalDevice.init(instance, [&](const PhysicalDevice& device) {
            const std::vector<std::string> missing = findMissingRequirements(device, requested);
            if (missing.empty()) return 1;
            VkPhysicalDeviceProperties properties;
            vkGetPhysicalDeviceProperties(device.get(), &properties);
            std::string list;
            for (const std::string& requirement : missing) list += (list.empty() ? "" : ", ") + requirement;
            rejections.push_back(std::format("{} lacks {}", properties.deviceName, list));
            return 0;
        });
    } catch (const std::runtime_error&) {
        if (rejections.empty()) throw;
        std::string message = "Failed to locate a suitable GPU:";
        for (const std::string& rejection : rejections) message += "\n  " + rejection;
        throw std::runtime_error(message);
    }
}

std::vector<std::string>
Device::findMissingRequirements(const PhysicalDevice& device, const DeviceFeatureChain& requested) const {
    const DeviceFeatureChain supported(device.get());
    std::vector<std::string> missing = requested.findMissing(supported);

    if (!findQueueFamilies(device, headless ? nullptr : &surface).isComplete(headless))
        missing.emplace_back(headless ? "graphics and transfer queues" : "graphics, transfer and present queues");
    if (!headless && missing.empty()) {
        const SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device, surface);
        if (swapChainSupport.formats.empty() || swapChainSupport.presentModes.empty())
            missing.emplace_back("surface formats and present modes");
    }
    return missing;
}
}
