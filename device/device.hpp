#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <functional>
#include <map>
#include <optional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "../instance/instance.hpp"
#include "../instance/debug_messenger.hpp"
#include "physical_device.hpp"
#include "logical_device.hpp"
#include "queue.hpp"
#include "../command/command_pool.hpp"
#include "../command/command_buffer.hpp"
#include "../present/surface.hpp"
#include "../memory/buffer.hpp"
#include "../image/image.hpp"
#include "../image/image_view.hpp"
#include "../image/sampler.hpp"
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

    Device(const std::string& appName, uint32_t appVersion, std::optional<const Window&> window = std::nullopt);
    ~Device();

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) = delete;
    Device& operator=(Device&&) = delete;

    bool isHeadless() const { return headless; }

    const Instance&           getInstance() const { return instance; }
    const PhysicalDevice&     getPhysicalDevice() const { return physicalDevice; }
    const LogicalDevice&      getLogicalDevice() const { return logicalDevice; }
    const QueueFamilyIndices& getQueueFamilies() const { return queueFamilies; }
    const Surface&            getSurface() const { return surface; }
    Queue&                    getGraphicsQueue() { return graphicsQueue; }
    Queue&                    getPresentQueue() { return presentQueue; }

    void setShaderFileSystem(std::shared_ptr<ShaderFileSystem> fileSystem) { shaderFileSystem = std::move(fileSystem); }
    ShaderLocation getShaderLocation(const std::string& shaderPath, const std::string& parameterName, const ShaderDefines& defines = {});
    const CompiledShader& compileShader(const std::string& path, const ShaderDefines& defines);
    void markShadersStale();

    CommandBuffer allocCommandBuffer();
    void freeCommandBuffer(CommandBuffer& commandBuffer);
    void immediateSubmit(const std::function<void(const CommandBuffer&)>& record);

    Buffer createBuffer(VkBufferUsageFlags usage, size_t size, BufferAllocFlags flags = 0, void* data = nullptr, std::string name = {});
    void destroyBuffer(Buffer& buffer);

    Image createImage(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, std::string name = {});
    Image createImageFromPath(std::string path, std::string name = {});
    Image createImageFromData(const void* data, size_t size, VkFormat format, uint32_t width, uint32_t height, std::string name = {});
    Image createEmptyImage(VkFormat format, uint32_t width, uint32_t height, std::string name = {});
    void fillImage(Image& image, const void* data, size_t size);
    void readImage(Image& image, void* out, size_t size);
    void copyImage(Image& src, Image& dst);
    void destroyImage(Image& image);

    ImageView createImageView(Image& image, std::string name = {});
    void destroyImageView(ImageView& imageView);

    Sampler createSampler(std::string name = {});
    void destroySampler(Sampler& sampler);

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

    Instance       instance;
    DebugMessenger debugMessenger;
    PhysicalDevice physicalDevice;
    Surface        surface;
    LogicalDevice  logicalDevice;

    QueueFamilyIndices queueFamilies;
    Queue graphicsQueue, transferQueue, presentQueue;
    CommandPool graphicsCommandPool, transferCommandPool, immediateCommandPool;

    uint32_t currentRetirementFrame = 0;
    std::array<std::vector<std::function<void(const LogicalDevice&)>>, MAX_FRAME_IN_FLIGHT> retirementQueue;

    std::map<std::pair<std::string, ShaderDefines>, CompiledShader> compiledShaders;
    std::shared_ptr<ShaderFileSystem> shaderFileSystem = std::make_shared<NativeShaderFileSystem>();

    void flushRetired();

    std::vector<const char*> getRequiredInstanceExtensions(std::optional<const Window&> window) const;
    std::vector<const char*> getRequiredDeviceExtensions() const;
    int scoreDevice(const PhysicalDevice& device) const;
};

}
