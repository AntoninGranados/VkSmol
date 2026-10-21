#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

#include "./device/device.hpp"

#include "./command/pending_transfers.hpp"

#include "./graph/builder_resource.hpp"

#include "./present/window.hpp"
#include "./present/swapchain.hpp"

#include "./render/shader_file_system.hpp"

#include "./resource/resource_registry.hpp"

#include "./sync/fence.hpp"
#include "./sync/semaphore.hpp"

#include "./utils.hpp"

namespace VkSmol {

class Engine {
public:
    Engine(
        const std::string& appName, uint32_t appVersion, std::optional<Window&> window = std::nullopt,
        EngineFeatures features = {}
    );
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) = delete;
    Engine& operator=(Engine&&) = delete;

    bool isHeadless() const { return !window.has_value(); }
    const EngineFeatures& getFeatures() const { return device.getFeatures(); }

    Device& getDevice() { return device; }
    ResourceRegistry& getResources() { return resources; }

    void setShaderFileSystem(std::shared_ptr<ShaderFileSystem> fileSystem) { device.setShaderFileSystem(std::move(fileSystem)); }
    std::vector<ShaderStruct> getShaderStructs(const std::string& shaderPath, const std::string& interfaceName) {
        return device.reflectShaderStructs(shaderPath, interfaceName);
    }

    VkExtent2D getExtent() const { return swapchain ? swapchain->getExtent() : VkExtent2D{}; }
    VkFormat getSwapchainFormat() const { return swapchain ? swapchain->getImageFormat() : VK_FORMAT_UNDEFINED; }
    ImageHandle getSwapchainImage() const { return swapchainImage; }
    uint32_t getFrame() const { return currentFrame; }

    void waitIdle() { device.waitIdle(); }

    ImageHandle createImage(
        std::string name, VkFormat format, uint32_t width, uint32_t height, uint32_t layers = 1, uint32_t mips = 1
    );
    ImageHandle importImage(std::string name, VkImage image, VkImageView view, VkFormat format, uint32_t width, uint32_t height,
                            ImageAccessInfo currentAccess = {});
    ImageHandle loadImage(const std::string& path);
    void destroyImage(ImageHandle image);
    void resizeImage(ImageHandle image, uint32_t width, uint32_t height);
    void swapImages(ImageHandle a, ImageHandle b);

    const std::string& getImageName(ImageHandle image) const;
    VkExtent2D getImageExtent(ImageHandle image, uint32_t mip = 0) const;
    uint32_t getImageLayerCount(ImageHandle image) const;
    uint32_t getImageMipCount(ImageHandle image) const;
    VkFormat    getImageFormat(ImageHandle image) const;
    VkImage     getImage(ImageHandle image) const;
    VkImageView getImageView(ImageRange image, std::optional<VkImageViewType> type = std::nullopt);
    VkSampler   getSampler(ImageHandle image) const;

    ImageSubresourceRange resolveImageRange(ImageRange image) const;

    void writeImage(ImageRange image, const void* data, size_t size);
    template <std::ranges::contiguous_range R> void writeImage(ImageRange image, const R& values) {
        writeImage(image, std::ranges::data(values), std::ranges::size(values) * sizeof(std::ranges::range_value_t<R>));
    }

    void readImage(ImageRange image, void* out, size_t size);
    template <std::ranges::contiguous_range R> void readImage(ImageRange image, R& out) {
        readImage(image, std::ranges::data(out), std::ranges::size(out) * sizeof(std::ranges::range_value_t<R>));
    }

    void copyImage(ImageRange source, ImageRange destination);

    BufferHandle createBuffer(std::string name, size_t size, const void* initialData = nullptr);
    BufferHandle importBuffer(std::string name, VkBuffer buffer, size_t size);
    void destroyBuffer(BufferHandle buffer);
    void resizeBuffer(BufferHandle buffer, size_t size);
    void swapBuffers(BufferHandle a, BufferHandle b);

    const std::string& getBufferName(BufferHandle buffer) const;
    size_t   getBufferSize(BufferHandle buffer) const;
    VkBuffer getBuffer(BufferHandle buffer) const;

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

    void copyBuffer(BufferHandle source, BufferHandle destination, size_t size, size_t sourceOffset = 0, size_t destinationOffset = 0);

    void flush();

private:
    std::optional<Window&> window;
    Device device;
    const LogicalDevice& logicalDevice;
    ResourceRegistry resources;
    PendingTransfers pendingTransfers;
    std::optional<Swapchain> swapchain;
    ImageHandle swapchainImage;

    std::vector<Semaphore> imageAvailableSemaphores;
    std::vector<Semaphore> renderFinishedSemaphores;
    bool framebufferResized = false;
    uint32_t currentFrame = 0;

    bool slotStarted = false;
    std::array<std::vector<const Fence*>, MAX_FRAME_IN_FLIGHT> slotFences;

    void beginSlot();
    void registerFence(uint32_t slot, const Fence& fence);
    void unregisterFence(const Fence& fence);

    std::optional<uint32_t> acquireImage();
    void presentImage(uint32_t imageIndex);
    void recreateSwapchain();
    void createPresentSemaphores();
    void destroyPresentSemaphores();

    friend class RenderGraph;
};

}
