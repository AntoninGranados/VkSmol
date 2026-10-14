#pragma once

#include <vulkan/vulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <type_traits>
#include <vector>

#include "./device/device.hpp"

#include "./graph/builder_resource.hpp"

#include "./present/window.hpp"
#include "./present/swapchain.hpp"

#include "./render/descriptor/descriptor_pool.hpp"
#include "./render/shader_file_system.hpp"

#include "./sync/fence.hpp"
#include "./sync/semaphore.hpp"

#include "./utils.hpp"

namespace VkSmol {

class Engine {
public:
    Engine(const std::string& appName, uint32_t appVersion, std::optional<Window&> window = std::nullopt);
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) = delete;
    Engine& operator=(Engine&&) = delete;

    bool isHeadless() const { return !window.has_value(); }

    Device& getDevice() { return device; }

    void setShaderFileSystem(std::shared_ptr<ShaderFileSystem> fileSystem) { device.setShaderFileSystem(std::move(fileSystem)); }

    VkExtent2D getExtent() const { return swapchain ? swapchain->getExtent() : VkExtent2D{}; }
    uint32_t getFrame() const { return currentFrame; }

    void waitIdle() { device.waitIdle(); }

    ImageHandle createImage(std::string name, VkFormat format, uint32_t width, uint32_t height);
    ImageHandle loadImage(const std::string& path);
    VkExtent2D getImageExtent(ImageHandle image) const;
    void writeImage(ImageHandle image, const void* data, size_t size);
    void readImage(ImageHandle image, void* out, size_t size);

    BufferHandle createBuffer(std::string name, size_t size, const void* initialData = nullptr);
    size_t getBufferSize(BufferHandle buffer) const;

    void writeBuffer(BufferHandle buffer, const void* data, size_t size, size_t offset = 0);
    template <typename T>
        requires(std::is_trivially_copyable_v<T> && !std::ranges::range<T>)
    void writeBuffer(BufferHandle buffer, const T& value, size_t offset = 0) {
        writeBuffer(buffer, &value, sizeof(T), offset);
    }
    template <std::ranges::contiguous_range R>
    void writeBuffer(BufferHandle buffer, const R& values, size_t offset = 0) {
        writeBuffer(buffer, std::ranges::data(values), std::ranges::size(values) * sizeof(std::ranges::range_value_t<R>), offset);
    }

    void readBuffer(BufferHandle buffer, void* out, size_t size, size_t offset = 0);
    template <std::ranges::contiguous_range R>
    void readBuffer(BufferHandle buffer, R& out, size_t offset = 0) {
        readBuffer(buffer, std::ranges::data(out), std::ranges::size(out) * sizeof(std::ranges::range_value_t<R>), offset);
    }

private:
    struct PersistentImage {
        std::string     name;
        VkFormat        format;
        uint32_t        width, height;
        Image           image;
        ImageView       view;
        Sampler         sampler;
        ImageAccessInfo lastAccess;
    };

    struct PersistentBuffer {
        std::string                     name;
        size_t                          size;
        Buffer                          buffer;
        std::optional<BufferAccessInfo> lastAccess;
    };

    std::optional<Window&> window;
    Device device;
    const LogicalDevice& logicalDevice;
    std::optional<Swapchain> swapchain;

    std::vector<Semaphore> imageAvailableSemaphores;
    bool framebufferResized = false;
    uint32_t currentFrame = 0;

    DescriptorPool uiDescriptorPool;
    VkFormat uiColorFormat = VK_FORMAT_UNDEFINED;

    std::vector<PersistentImage>  images;
    std::vector<PersistentBuffer> buffers;

    bool slotStarted = false;
    std::array<std::vector<const Fence*>, MAX_FRAME_IN_FLIGHT> slotFences;

    void beginSlot();
    void registerFence(uint32_t slot, const Fence& fence);
    void unregisterFence(const Fence& fence);

    void transitionImage(const CommandBuffer& commandBuffer, PersistentImage& image, ImageAccessInfo access);
    void transitionBuffer(const CommandBuffer& commandBuffer, PersistentBuffer& buffer, BufferAccessInfo access);

    std::optional<uint32_t> acquireImage();
    void presentImage(uint32_t imageIndex, const Semaphore& waitSemaphore);
    void recreateSwapchain();

    void initImgui();

    friend class RenderGraph;
};

}
