#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <functional>
#include <map>
#include <optional>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "../instance/instance.hpp"
#include "device_features.hpp"
#include "engine_features.hpp"
#include "physical_device.hpp"
#include "logical_device.hpp"
#include "queue.hpp"
#include "../command/command_buffer.hpp"
#include "../present/surface.hpp"
#include "../memory/buffer.hpp"
#include "../image/image.hpp"
#include "../render/shader.hpp"
#include "../render/shader_file_system.hpp"
#include "../utils.hpp"

namespace VkSmol {

class Window;

class Device {
public:
    struct CompiledShader {
        ShaderCompilation compilation;
        std::string error;
        bool checked = false;
    };

    Device(
        const std::string& appName, uint32_t appVersion, std::optional<const Window&> window = std::nullopt,
        EngineFeatures features = {}
    );
    ~Device();

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) = delete;
    Device& operator=(Device&&) = delete;

    bool isHeadless() const { return headless; }
    const EngineFeatures& getFeatures() const { return features; }

    const Instance&           getInstance() const { return instance; }
    const PhysicalDevice&     getPhysicalDevice() const { return physicalDevice; }
    const LogicalDevice&      getLogicalDevice() const { return logicalDevice; }
    const QueueFamilyIndices& getQueueFamilies() const { return queueFamilies; }
    const Surface&            getSurface() const { return surface; }
    Queue&                    getGraphicsQueue() { return graphicsQueue; }
    Queue&                    getPresentQueue() { return presentQueue; }

    void setShaderFileSystem(std::shared_ptr<ShaderFileSystem> fileSystem) { shaderFileSystem = std::move(fileSystem); }
    const CompiledShader& compileShader(
        const std::vector<std::string>& paths, const ShaderDefines& defines, const ShaderConformances& conformances
    );
    std::vector<ShaderStruct> reflectShaderStructs(const std::string& path, const std::string& interfaceName) {
        return Shader::reflect(path, interfaceName, *shaderFileSystem);
    }
    void markShadersStale();

    CommandBuffer allocCommandBuffer();
    void freeCommandBuffer(CommandBuffer& commandBuffer);
    void immediateSubmit(const std::function<void(const CommandBuffer&)>& record);

    Buffer createBuffer(VkBufferUsageFlags usage, size_t size, BufferAllocFlags flags = 0);
    void destroyBuffer(Buffer& buffer);

    Image createImage(
        uint32_t width, uint32_t height, uint32_t mipLevels, uint32_t arrayLayers, VkFormat format,
        VkImageUsageFlags usage, VkMemoryPropertyFlags properties
    );
    void destroyImage(Image& image);

    ImageView createImageView(Image& image, VkImageViewType type, const VkImageSubresourceRange& range);
    void destroyImageView(ImageView& imageView);

    Sampler createSampler();
    void destroySampler(Sampler& sampler);

    void setObjectName(VkObjectType type, uint64_t handle, const std::string& name) const;
    void beginDebugLabel(const CommandBuffer& commandBuffer, const std::string& name) const;
    void endDebugLabel(const CommandBuffer& commandBuffer) const;

    void emitMemoryBarrier(
        const CommandBuffer& commandBuffer,
        VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess,
        VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess
    ) const;

    void retire(std::function<void(const LogicalDevice&)> payload);
    void collectRetired(uint32_t frame);
    void waitIdle();

private:
    bool headless;
    EngineFeatures features;

    Instance       instance;
    DebugMessenger debugMessenger;
    PhysicalDevice physicalDevice;
    Surface        surface;
    LogicalDevice  logicalDevice;

    PFN_vkSetDebugUtilsObjectNameEXT setDebugUtilsObjectName = nullptr;
    PFN_vkCmdBeginDebugUtilsLabelEXT cmdBeginDebugUtilsLabel = nullptr;
    PFN_vkCmdEndDebugUtilsLabelEXT cmdEndDebugUtilsLabel = nullptr;

    QueueFamilyIndices queueFamilies;
    Queue graphicsQueue, transferQueue, presentQueue;
    CommandPool graphicsCommandPool, transferCommandPool, immediateCommandPool;

    uint32_t currentRetirementFrame = 0;
    std::array<std::vector<std::function<void(const LogicalDevice&)>>, MAX_FRAME_IN_FLIGHT> retirementQueue;

    std::map<std::tuple<std::vector<std::string>, ShaderDefines, ShaderConformances>, CompiledShader> compiledShaders;
    std::shared_ptr<ShaderFileSystem> shaderFileSystem = std::make_shared<NativeShaderFileSystem>();

    void flushRetired();

    std::vector<const char*> getRequiredInstanceExtensions(std::optional<const Window&> window) const;
    void selectPhysicalDevice(const DeviceFeatureChain& requested);
    std::vector<std::string>
    findMissingRequirements(const PhysicalDevice& device, const DeviceFeatureChain& requested) const;
};

}
