#pragma once

#include <vulkan/vulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cstddef>
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
    VkFormat getSwapchainFormat() const { return swapchain ? swapchain->getImageFormat() : VK_FORMAT_UNDEFINED; }
    ImageHandle getSwapchainImage() const { return swapchainImage; }
    uint32_t getFrame() const { return currentFrame; }

    void waitIdle() { device.waitIdle(); }

    ImageHandle createImage(std::string name, VkFormat format, uint32_t width, uint32_t height);
    ImageHandle importImage(std::string name, VkImage image, VkImageView view, VkFormat format, uint32_t width, uint32_t height,
                            ImageAccessInfo currentAccess = {});
    ImageHandle loadImage(const std::string& path);
    void destroyImage(ImageHandle image);
    void resizeImage(ImageHandle image, uint32_t width, uint32_t height);
    void swapImages(ImageHandle a, ImageHandle b);

    const std::string& getImageName(ImageHandle image) const;
    VkExtent2D  getImageExtent(ImageHandle image) const;
    VkFormat    getImageFormat(ImageHandle image) const;
    VkImage     getImage(ImageHandle image) const;
    VkImageView getImageView(ImageHandle image) const;
    VkSampler   getSampler(ImageHandle image) const;

    void writeImage(ImageHandle image, const void* data, size_t size);
    template <std::ranges::contiguous_range R>
    void writeImage(ImageHandle image, const R& values) {
        writeImage(image, std::ranges::data(values), std::ranges::size(values) * sizeof(std::ranges::range_value_t<R>));
    }

    void readImage(ImageHandle image, void* out, size_t size);
    template <std::ranges::contiguous_range R>
    void readImage(ImageHandle image, R& out) {
        readImage(image, std::ranges::data(out), std::ranges::size(out) * sizeof(std::ranges::range_value_t<R>));
    }

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

    void flush();

private:
    struct PersistentImage {
        std::string     name;
        VkFormat        format = VK_FORMAT_UNDEFINED;
        uint32_t        width = 0, height = 0;
        VkImage         image   = VK_NULL_HANDLE;
        VkImageView     view    = VK_NULL_HANDLE;
        VkSampler       sampler = VK_NULL_HANDLE;
        ImageAccessInfo lastAccess;
        bool            imported = false;
        Image           ownedImage;
        ImageView       ownedView;
        Sampler         ownedSampler;
    };

    struct PersistentBuffer {
        std::string                     name;
        size_t                          size = 0;
        VkBuffer                        buffer = VK_NULL_HANDLE;
        std::optional<BufferAccessInfo> lastAccess;
        bool                            imported = false;
        Buffer                          ownedBuffer;
    };

    struct PendingWrite {
        bool                   image;
        uint32_t               id;
        size_t                 offset;
        std::vector<std::byte> data;
    };

    std::optional<Window&> window;
    Device device;
    const LogicalDevice& logicalDevice;
    std::optional<Swapchain> swapchain;
    ImageHandle swapchainImage;

    std::vector<Semaphore> imageAvailableSemaphores;
    std::vector<Semaphore> renderFinishedSemaphores;
    bool framebufferResized = false;
    uint32_t currentFrame = 0;

    std::vector<PersistentImage>  images;
    std::vector<PersistentBuffer> buffers;
    std::vector<uint32_t>         freeImages;
    std::vector<uint32_t>         freeBuffers;
    std::vector<PendingWrite>     pendingWrites;

    bool slotStarted = false;
    std::array<std::vector<const Fence*>, MAX_FRAME_IN_FLIGHT> slotFences;

    void beginSlot();
    void registerFence(uint32_t slot, const Fence& fence);
    void unregisterFence(const Fence& fence);

    ImageHandle  insertImage(PersistentImage&& image);
    BufferHandle insertBuffer(PersistentBuffer&& buffer);
    void allocateImage(PersistentImage& image);
    void releaseImage(PersistentImage& image);
    void allocateBuffer(PersistentBuffer& buffer);
    void releaseBuffer(PersistentBuffer& buffer);
    void dropPendingWrites(bool image, uint32_t id);
    void flushWrites(const CommandBuffer& commandBuffer);

    void transitionImage(const CommandBuffer& commandBuffer, PersistentImage& image, ImageAccessInfo access);
    void transitionBuffer(const CommandBuffer& commandBuffer, PersistentBuffer& buffer, BufferAccessInfo access);

    std::optional<uint32_t> acquireImage();
    void presentImage(uint32_t imageIndex);
    void recreateSwapchain();
    void createPresentSemaphores();
    void destroyPresentSemaphores();

    friend class RenderGraph;
};

}
