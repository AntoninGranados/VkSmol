#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "../graph/builder_resource.hpp"
#include "../image/image.hpp"
#include "../image/image_subresource.hpp"
#include "../image/image_view_cache.hpp"
#include "../image/sampler.hpp"
#include "../memory/buffer.hpp"
#include "../track/resource_state.hpp"

namespace VkSmol {

class CommandBuffer;
class Device;

struct ImageResource {
    std::string name;
    VkFormat format = VK_FORMAT_UNDEFINED;
    uint32_t width = 0, height = 0;
    ImageSubresourceLayout layout;
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    ImageState state;
    bool imported = false;
    Image ownedImage;
    ImageViewCache ownedViews;
    Sampler ownedSampler;

    VkExtent2D getExtent() const { return {width, height}; }
};

struct BufferResource {
    std::string name;
    size_t size = 0;
    VkBuffer buffer = VK_NULL_HANDLE;
    BufferState state;
    bool imported = false;
    Buffer ownedBuffer;
};

class ResourceRegistry {
public:
    explicit ResourceRegistry(Device& device);
    ~ResourceRegistry();

    ResourceRegistry(const ResourceRegistry&) = delete;
    ResourceRegistry& operator=(const ResourceRegistry&) = delete;

    ImageHandle
    createImage(std::string name, VkFormat format, uint32_t width, uint32_t height, uint32_t layers, uint32_t mips);
    ImageHandle importImage(
        std::string name, VkImage image, VkImageView view, VkFormat format, uint32_t width, uint32_t height,
        ImageAccessInfo currentAccess
    );
    void destroyImage(ImageHandle image);
    bool resizeImage(ImageHandle image, uint32_t width, uint32_t height);
    void swapImages(ImageHandle a, ImageHandle b);

    BufferHandle createBuffer(std::string name, size_t size);
    BufferHandle importBuffer(std::string name, VkBuffer buffer, size_t size);
    void destroyBuffer(BufferHandle buffer);
    bool resizeBuffer(BufferHandle buffer, size_t size);
    void swapBuffers(BufferHandle a, BufferHandle b);

    ImageResource& getImage(ImageHandle image) { return images[image.id]; }
    const ImageResource& getImage(ImageHandle image) const { return images[image.id]; }
    BufferResource& getBuffer(BufferHandle buffer) { return buffers[buffer.id]; }
    const BufferResource& getBuffer(BufferHandle buffer) const { return buffers[buffer.id]; }

    ImageSubresourceRange resolve(ImageRange image) const;
    VkImageView getImageView(ImageRange image, std::optional<VkImageViewType> type);

    void transitionImage(const CommandBuffer& commandBuffer, ImageRange image, const ImageAccessInfo& access);
    void transitionBuffer(const CommandBuffer& commandBuffer, BufferHandle buffer, const BufferAccessInfo& access);

private:
    Device& device;

    std::vector<ImageResource> images;
    std::vector<BufferResource> buffers;
    std::vector<uint32_t> freeImages;
    std::vector<uint32_t> freeBuffers;

    ImageHandle insertImage(ImageResource&& image);
    BufferHandle insertBuffer(BufferResource&& buffer);
    void allocateImage(ImageResource& image);
    void releaseImage(ImageResource& image);
    void allocateBuffer(BufferResource& buffer);
    void releaseBuffer(BufferResource& buffer);
};

} // namespace VkSmol
