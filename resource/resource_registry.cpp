#include "resource_registry.hpp"

#include <cassert>
#include <format>
#include <stdexcept>
#include <utility>

#include "../command/command_buffer.hpp"
#include "../device/device.hpp"

namespace VkSmol {

namespace {

constexpr VkBufferUsageFlags kBufferUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                            VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                            VK_BUFFER_USAGE_TRANSFER_DST_BIT;

VkImageUsageFlags supportedImageUsage(const PhysicalDevice& physicalDevice, VkFormat format) {
    VkFormatProperties properties;
    vkGetPhysicalDeviceFormatProperties(physicalDevice.get(), format, &properties);
    const VkFormatFeatureFlags features = properties.optimalTilingFeatures;

    VkImageUsageFlags usage = 0;
    if (features & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (features & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    if (features & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (features & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT) usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (features & VK_FORMAT_FEATURE_TRANSFER_DST_BIT) usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    return usage;
}

void checkImageLayout(const std::string& name, VkExtent2D extent, const ImageSubresourceLayout& layout) {
    if (layout.layerCount == 0 || layout.mipCount == 0 || layout.mipCount > maxMipCount(extent))
        throw std::runtime_error(
            std::format(
                "The image [{}] of {}x{} cannot have {} layers and {} mips", name, extent.width, extent.height,
                layout.layerCount, layout.mipCount
            )
        );
}

} // namespace

ResourceRegistry::ResourceRegistry(Device& device) : device(device) {}

ResourceRegistry::~ResourceRegistry() {
    for (ImageResource& image : images) {
        releaseImage(image);
        device.destroySampler(image.ownedSampler);
    }
    for (BufferResource& buffer : buffers) releaseBuffer(buffer);
}

// ——— Images ——————————————————————————————————————————————————————————————————

ImageHandle ResourceRegistry::createImage(
    std::string name, VkFormat format, uint32_t width, uint32_t height, uint32_t layers, uint32_t mips
) {
    ImageResource image = {
        .name = std::move(name),
        .format = format,
        .width = width,
        .height = height,
        .layout = {.mipCount = mips, .layerCount = layers},
    };
    checkImageLayout(image.name, image.getExtent(), image.layout);
    allocateImage(image);
    return insertImage(std::move(image));
}

ImageHandle ResourceRegistry::importImage(
    std::string name, VkImage handle, VkImageView view, VkFormat format, uint32_t width, uint32_t height,
    ImageAccessInfo currentAccess
) {
    ImageResource image = {
        .name = std::move(name),
        .format = format,
        .width = width,
        .height = height,
        .image = handle,
        .view = view,
        .imported = true,
    };
    image.state.reset(image.layout, currentAccess);
    image.ownedSampler = device.createSampler(image.name + "Sampler");
    image.sampler = image.ownedSampler.get();
    return insertImage(std::move(image));
}

void ResourceRegistry::destroyImage(ImageHandle handle) {
    ImageResource& image = images[handle.id];
    assert(image.image != VK_NULL_HANDLE);
    releaseImage(image);
    device.destroySampler(image.ownedSampler);
    image = {};
    freeImages.push_back(handle.id);
}

bool ResourceRegistry::resizeImage(ImageHandle handle, uint32_t width, uint32_t height) {
    ImageResource& image = images[handle.id];
    if (image.imported)
        throw std::runtime_error(std::format("Cannot resize the image [{}] as it is imported", image.name));
    if (image.width == width && image.height == height) return false;
    checkImageLayout(image.name, {width, height}, image.layout);

    releaseImage(image);
    image.width = width;
    image.height = height;
    allocateImage(image);
    return true;
}

void ResourceRegistry::swapImages(ImageHandle a, ImageHandle b) {
    ImageResource& first = images[a.id];
    ImageResource& second = images[b.id];
    std::swap(first.name, second.name);
    std::swap(first, second);
}

// ——— Buffers —————————————————————————————————————————————————————————————————

BufferHandle ResourceRegistry::createBuffer(std::string name, size_t size) {
    BufferResource buffer = {.name = std::move(name), .size = size};
    allocateBuffer(buffer);
    return insertBuffer(std::move(buffer));
}

BufferHandle ResourceRegistry::importBuffer(std::string name, VkBuffer handle, size_t size) {
    return insertBuffer({.name = std::move(name), .size = size, .buffer = handle, .imported = true});
}

void ResourceRegistry::destroyBuffer(BufferHandle handle) {
    BufferResource& buffer = buffers[handle.id];
    assert(buffer.buffer != VK_NULL_HANDLE);
    releaseBuffer(buffer);
    buffer = {};
    freeBuffers.push_back(handle.id);
}

bool ResourceRegistry::resizeBuffer(BufferHandle handle, size_t size) {
    BufferResource& buffer = buffers[handle.id];
    if (buffer.imported)
        throw std::runtime_error(std::format("Cannot resize the buffer [{}] as it is imported", buffer.name));
    if (buffer.size == size) return false;

    releaseBuffer(buffer);
    buffer.size = size;
    allocateBuffer(buffer);
    return true;
}

void ResourceRegistry::swapBuffers(BufferHandle a, BufferHandle b) {
    BufferResource& first = buffers[a.id];
    BufferResource& second = buffers[b.id];
    std::swap(first.name, second.name);
    std::swap(first, second);
}

// ——— Access ——————————————————————————————————————————————————————————————————

ImageSubresourceRange ResourceRegistry::resolve(ImageRange range) const {
    const ImageResource& image = images[range.image.id];
    const std::optional<ImageSubresourceRange> resolved = image.layout.resolve(range.subresources);
    if (!resolved)
        throw std::runtime_error(
            std::format(
                "The subresources [{}] are out of the image [{}] of {} mips and {} layers",
                range.subresources.toString(), image.name, image.layout.mipCount, image.layout.layerCount
            )
        );
    return *resolved;
}

VkImageView ResourceRegistry::getImageView(ImageRange range, std::optional<VkImageViewType> type) {
    ImageResource& image = images[range.image.id];
    const ImageSubresourceRange subresources = resolve(range);
    const VkImageViewType viewType =
        type.value_or(subresources.layerCount > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D);
    if (!image.imported) return image.ownedViews.getOrCreate(device, image.ownedImage, viewType, subresources);

    if (viewType != VK_IMAGE_VIEW_TYPE_2D)
        throw std::runtime_error(std::format("The imported image [{}] only provides a 2D view", image.name));
    return image.view;
}

void ResourceRegistry::transitionImage(
    const CommandBuffer& commandBuffer, ImageRange range, const ImageAccessInfo& access
) {
    ImageResource& image = images[range.image.id];
    const std::vector<ImageTransition> transitions = image.state.transition(image.layout, resolve(range), access);
    if (transitions.empty()) return;

    std::vector<VkImageMemoryBarrier2> barriers;
    for (const ImageTransition& transition : transitions) barriers.push_back(transition.toVk(image.image));
    const VkDependencyInfo dependency{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = static_cast<uint32_t>(barriers.size()),
        .pImageMemoryBarriers = barriers.data(),
    };
    device.getLogicalDevice().functions().cmdPipelineBarrier2(commandBuffer.get(), &dependency);
}

void ResourceRegistry::transitionBuffer(
    const CommandBuffer& commandBuffer, BufferHandle handle, const BufferAccessInfo& access
) {
    BufferResource& buffer = buffers[handle.id];
    const std::optional<BufferTransition> transition = buffer.state.transition(access);
    if (!transition) return;

    const VkBufferMemoryBarrier2 barrier = transition->toVk(buffer.buffer);
    const VkDependencyInfo dependency{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .bufferMemoryBarrierCount = 1,
        .pBufferMemoryBarriers = &barrier,
    };
    device.getLogicalDevice().functions().cmdPipelineBarrier2(commandBuffer.get(), &dependency);
}

// ——— Storage —————————————————————————————————————————————————————————————————

ImageHandle ResourceRegistry::insertImage(ImageResource&& image) {
    if (freeImages.empty()) {
        images.push_back(std::move(image));
        return {.id = static_cast<uint32_t>(images.size() - 1)};
    }
    const uint32_t id = freeImages.back();
    freeImages.pop_back();
    images[id] = std::move(image);
    return {.id = id};
}

BufferHandle ResourceRegistry::insertBuffer(BufferResource&& buffer) {
    if (freeBuffers.empty()) {
        buffers.push_back(std::move(buffer));
        return {.id = static_cast<uint32_t>(buffers.size() - 1)};
    }
    const uint32_t id = freeBuffers.back();
    freeBuffers.pop_back();
    buffers[id] = std::move(buffer);
    return {.id = id};
}

void ResourceRegistry::allocateImage(ImageResource& image) {
    const VkImageUsageFlags usage = supportedImageUsage(device.getPhysicalDevice(), image.format);
    image.ownedImage = device.createImage(
        image.width, image.height, image.layout.mipCount, image.layout.layerCount, image.format, usage,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image.name
    );
    if (!image.ownedSampler.isValid()) image.ownedSampler = device.createSampler(image.name + "Sampler");

    image.image = image.ownedImage.get();
    image.sampler = image.ownedSampler.get();
    image.state.reset(image.layout);
}

void ResourceRegistry::releaseImage(ImageResource& image) {
    image.ownedViews.destroy(device);
    device.destroyImage(image.ownedImage);
    image.image = VK_NULL_HANDLE;
    image.view = VK_NULL_HANDLE;
}

void ResourceRegistry::allocateBuffer(BufferResource& buffer) {
    buffer.ownedBuffer = device.createBuffer(kBufferUsage, buffer.size, 0, buffer.name);
    buffer.buffer = buffer.ownedBuffer.get();
    buffer.state.reset();
}

void ResourceRegistry::releaseBuffer(BufferResource& buffer) {
    device.destroyBuffer(buffer.ownedBuffer);
    buffer.buffer = VK_NULL_HANDLE;
}

} // namespace VkSmol
