#include "engine.hpp"

#include <algorithm>
#include <cassert>
#include <format>
#include <stdexcept>

#include <stb_image/stb_image.h>

#include "graph/compiled_resource.hpp"
#include "graph/graph_utils.hpp"
#include "image/image_transition.hpp"

namespace VkSmol {

Engine::Engine(const std::string& appName, uint32_t appVersion, std::optional<Window&> window)
    : window(window), device(appName, appVersion, window), logicalDevice(device.getLogicalDevice()) {
    if (!window) return;

    swapchain.emplace(device, window->getExtent());
    swapchainImage = importImage("Swapchain", VK_NULL_HANDLE, VK_NULL_HANDLE, swapchain->getImageFormat(),
                                 swapchain->getExtent().width, swapchain->getExtent().height);
    createPresentSemaphores();
    window->setResizeCallback([this](int, int) { framebufferResized = true; });
}

Engine::~Engine() {
    device.waitIdle();

    for (PersistentImage& image : images) {
        releaseImage(image);
        device.destroySampler(image.ownedSampler);
    }
    for (PersistentBuffer& buffer : buffers) releaseBuffer(buffer);

    if (window) {
        destroyPresentSemaphores();
        window->setResizeCallback({});
    }
}

namespace {

constexpr VkBufferUsageFlags kBufferUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
    | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
    | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

constexpr size_t kStagingAlignment = 16;

VkImageUsageFlags supportedImageUsage(const PhysicalDevice& physicalDevice, VkFormat format) {
    VkFormatProperties properties;
    vkGetPhysicalDeviceFormatProperties(physicalDevice.get(), format, &properties);
    const VkFormatFeatureFlags features = properties.optimalTilingFeatures;

    VkImageUsageFlags usage = 0;
    if (features & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)    usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (features & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT)    usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    if (features & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (features & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT)     usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (features & VK_FORMAT_FEATURE_TRANSFER_DST_BIT)     usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    return usage;
}

size_t alignStaging(size_t offset) {
    return (offset + kStagingAlignment - 1) & ~(kStagingAlignment - 1);
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

size_t texelSize(const std::string& name, VkExtent2D extent, const ImageSubresourceRange& range, size_t size) {
    const size_t texels = texelCount(extent, range);
    if (size % texels != 0)
        throw std::runtime_error(
            std::format(
                "{} bytes do not tightly pack the {} texels of [{}] in the image [{}]", size, texels, range.toString(),
                name
            )
        );
    return size / texels;
}
}

// ——— Images ——————————————————————————————————————————————————————————————————

ImageHandle Engine::createImage(
    std::string name, VkFormat format, uint32_t width, uint32_t height, uint32_t layers, uint32_t mips
) {
    PersistentImage image = {
        .name = std::move(name),
        .format = format,
        .width = width,
        .height = height,
        .layout = {.mipCount = mips, .layerCount = layers},
    };
    checkImageLayout(image.name, {width, height}, image.layout);
    allocateImage(image);
    return insertImage(std::move(image));
}

ImageHandle Engine::importImage(std::string name, VkImage handle, VkImageView view, VkFormat format, uint32_t width, uint32_t height,
                                ImageAccessInfo currentAccess) {
    PersistentImage image = {
        .name = std::move(name),
        .format = format,
        .width = width,
        .height = height,
        .image = handle,
        .view = view,
        .accesses = {currentAccess},
        .imported = true,
    };
    image.ownedSampler = device.createSampler(image.name + "Sampler");
    image.sampler      = image.ownedSampler.get();
    return insertImage(std::move(image));
}

ImageHandle Engine::loadImage(const std::string& path) {
    int width, height, channels;
    stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels) throw std::runtime_error(std::format("Failed to load the image [{}]", path));

    const ImageHandle handle = createImage(path, VK_FORMAT_R8G8B8A8_UNORM, static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    writeImage(handle, pixels, static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
    stbi_image_free(pixels);
    return handle;
}

void Engine::destroyImage(ImageHandle handle) {
    PersistentImage& image = images[handle.id];
    assert(image.image != VK_NULL_HANDLE);
    dropPendingTransfers(true, handle.id);
    releaseImage(image);
    device.destroySampler(image.ownedSampler);
    image = {};
    freeImages.push_back(handle.id);
}

void Engine::resizeImage(ImageHandle handle, uint32_t width, uint32_t height) {
    PersistentImage& image = images[handle.id];
    if (image.imported)
        throw std::runtime_error(std::format("Cannot resize the image [{}] as it is imported", image.name));
    if (image.width == width && image.height == height) return;
    checkImageLayout(image.name, {width, height}, image.layout);

    dropPendingTransfers(true, handle.id);
    releaseImage(image);
    image.width  = width;
    image.height = height;
    allocateImage(image);
}

void Engine::swapImages(ImageHandle a, ImageHandle b) {
    PersistentImage& first  = images[a.id];
    PersistentImage& second = images[b.id];
    std::swap(first.name, second.name);
    std::swap(first, second);
    remapPendingTransfers(true, a.id, b.id);
}

const std::string& Engine::getImageName(ImageHandle handle) const {
    return images[handle.id].name;
}

VkExtent2D Engine::getImageExtent(ImageHandle handle, uint32_t mip) const {
    const PersistentImage& image = images[handle.id];
    if (mip >= image.layout.mipCount)
        throw std::runtime_error(
            std::format("The image [{}] has no mip {} (it has {})", image.name, mip, image.layout.mipCount)
        );
    return mipExtent({image.width, image.height}, mip);
}

uint32_t Engine::getImageLayerCount(ImageHandle handle) const { return images[handle.id].layout.layerCount; }

uint32_t Engine::getImageMipCount(ImageHandle handle) const { return images[handle.id].layout.mipCount; }

VkFormat Engine::getImageFormat(ImageHandle handle) const {
    return images[handle.id].format;
}

VkImage Engine::getImage(ImageHandle handle) const {
    return images[handle.id].image;
}

VkImageView Engine::getImageView(ImageRange range, std::optional<VkImageViewType> type) {
    PersistentImage& image = images[range.image.id];
    const ImageSubresourceRange subresources = resolveSubresources(image, range.subresources);
    const VkImageViewType viewType =
        type.value_or(subresources.layerCount > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D);
    if (!image.imported) return image.ownedViews.getOrCreate(device, image.ownedImage, viewType, subresources);

    if (viewType != VK_IMAGE_VIEW_TYPE_2D)
        throw std::runtime_error(std::format("The imported image [{}] only provides a 2D view", image.name));
    return image.view;
}

VkSampler Engine::getSampler(ImageHandle handle) const {
    return images[handle.id].sampler;
}

ImageSubresourceRange Engine::resolveImageRange(ImageRange range) const {
    return resolveSubresources(images[range.image.id], range.subresources);
}

void Engine::writeImage(ImageRange range, const void* data, size_t size) {
    if (size == 0) return;
    const PersistentImage& image = images[range.image.id];
    texelSize(image.name, {image.width, image.height}, resolveSubresources(image, range.subresources), size);
    const std::byte* bytes = static_cast<const std::byte*>(data);
    pendingTransfers.push_back(
        PendingWrite{
            .image = true,
            .destination = range.image.id,
            .offset = 0,
            .subresources = range.subresources,
            .data = std::vector<std::byte>(bytes, bytes + size),
        }
    );
}

void Engine::readImage(ImageRange range, void* out, size_t size) {
    if (size == 0) return;
    PersistentImage& image = images[range.image.id];
    const ImageSubresourceRange subresources = resolveSubresources(image, range.subresources);
    const VkExtent2D extent = {image.width, image.height};
    const std::vector<VkBufferImageCopy> regions =
        bufferImageCopyRegions(extent, subresources, 0, texelSize(image.name, extent, subresources, size));

    Buffer readback = device.createBuffer(VK_BUFFER_USAGE_TRANSFER_DST_BIT, size, BufferAllocMapped, nullptr, image.name + "Readback");
    device.immediateSubmit([&](const CommandBuffer& commandBuffer) {
        flushTransfers(commandBuffer);
        transitionImage(
            commandBuffer, image, subresources, {.usage = ImageUsageType::TransferSrc, .access = AccessType::Read}
        );
        vkCmdCopyImageToBuffer(
            commandBuffer.get(), image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback.get(),
            static_cast<uint32_t>(regions.size()), regions.data()
        );
        device.emitMemoryBarrier(commandBuffer,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT);
    });
    readback.read(logicalDevice, out, size);
    device.destroyBuffer(readback);
}

void Engine::copyImage(ImageRange source, ImageRange destination) {
    validateImageCopy(
        images[source.image.id], source.subresources, images[destination.image.id], destination.subresources
    );
    pendingTransfers.push_back(
        PendingCopy{
            .image = true,
            .source = source.image.id,
            .destination = destination.image.id,
            .sourceOffset = 0,
            .destinationOffset = 0,
            .size = 0,
            .sourceSubresources = source.subresources,
            .destinationSubresources = destination.subresources,
        }
    );
}

void Engine::validateImageCopy(
    const PersistentImage& source, ImageSubresourceRange sourceSubresources, const PersistentImage& destination,
    ImageSubresourceRange destinationSubresources
) const {
    const ImageSubresourceRange from = resolveSubresources(source, sourceSubresources);
    const ImageSubresourceRange to = resolveSubresources(destination, destinationSubresources);
    if (from.mipCount != to.mipCount || from.layerCount != to.layerCount)
        throw std::runtime_error(
            std::format(
                "Cannot copy {} mips and {} layers of the image [{}] into {} mips and {} layers of the image [{}]",
                from.mipCount, from.layerCount, source.name, to.mipCount, to.layerCount, destination.name
            )
        );
    if (&source == &destination && from.overlaps(to))
        throw std::runtime_error(std::format("Cannot copy overlapping subresources of the image [{}]", source.name));

    for (uint32_t i = 0; i < from.mipCount; ++i) {
        const VkExtent2D fromExtent = mipExtent({source.width, source.height}, from.baseMip + i);
        const VkExtent2D toExtent = mipExtent({destination.width, destination.height}, to.baseMip + i);
        if (fromExtent.width != toExtent.width || fromExtent.height != toExtent.height)
            throw std::runtime_error(
                std::format(
                    "Cannot copy mip {} of the image [{}] of {}x{} into mip {} of the image [{}] of {}x{}",
                    from.baseMip + i, source.name, fromExtent.width, fromExtent.height, to.baseMip + i,
                    destination.name, toExtent.width, toExtent.height
                )
            );
    }
}

// ——— Buffers —————————————————————————————————————————————————————————————————

BufferHandle Engine::createBuffer(std::string name, size_t size, const void* initialData) {
    PersistentBuffer buffer = { .name = std::move(name), .size = size };
    allocateBuffer(buffer);
    const BufferHandle handle = insertBuffer(std::move(buffer));
    if (initialData) writeBuffer(handle, initialData, size);
    return handle;
}

BufferHandle Engine::importBuffer(std::string name, VkBuffer handle, size_t size) {
    return insertBuffer({ .name = std::move(name), .size = size, .buffer = handle, .imported = true });
}

void Engine::destroyBuffer(BufferHandle handle) {
    PersistentBuffer& buffer = buffers[handle.id];
    assert(buffer.buffer != VK_NULL_HANDLE);
    dropPendingTransfers(false, handle.id);
    releaseBuffer(buffer);
    buffer = {};
    freeBuffers.push_back(handle.id);
}

void Engine::resizeBuffer(BufferHandle handle, size_t size) {
    PersistentBuffer& buffer = buffers[handle.id];
    if (buffer.imported)
        throw std::runtime_error(std::format("Cannot resize the buffer [{}] as it is imported", buffer.name));
    if (buffer.size == size) return;

    dropPendingTransfers(false, handle.id);
    releaseBuffer(buffer);
    buffer.size = size;
    allocateBuffer(buffer);
}

void Engine::swapBuffers(BufferHandle a, BufferHandle b) {
    PersistentBuffer& first  = buffers[a.id];
    PersistentBuffer& second = buffers[b.id];
    std::swap(first.name, second.name);
    std::swap(first, second);
    remapPendingTransfers(false, a.id, b.id);
}

const std::string& Engine::getBufferName(BufferHandle handle) const {
    return buffers[handle.id].name;
}

size_t Engine::getBufferSize(BufferHandle handle) const {
    return buffers[handle.id].size;
}

VkBuffer Engine::getBuffer(BufferHandle handle) const {
    return buffers[handle.id].buffer;
}

void Engine::writeBuffer(BufferHandle handle, const void* data, size_t size, size_t offset) {
    if (size == 0) return;
    const PersistentBuffer& buffer = buffers[handle.id];
    if (offset + size > buffer.size)
        throw std::runtime_error(std::format("Writing {} bytes at offset {} overflows the buffer [{}] of {} bytes", size, offset, buffer.name, buffer.size));

    const std::byte* bytes = static_cast<const std::byte*>(data);
    pendingTransfers.push_back(PendingWrite{ .image = false, .destination = handle.id, .offset = offset, .data = std::vector<std::byte>(bytes, bytes + size) });
}

void Engine::readBuffer(BufferHandle handle, void* out, size_t size, size_t offset) {
    if (size == 0) return;
    PersistentBuffer& buffer = buffers[handle.id];
    if (offset + size > buffer.size)
        throw std::runtime_error(std::format("Reading {} bytes at offset {} overflows the buffer [{}] of {} bytes", size, offset, buffer.name, buffer.size));

    Buffer readback = device.createBuffer(VK_BUFFER_USAGE_TRANSFER_DST_BIT, size, BufferAllocMapped, nullptr, buffer.name + "Readback");
    device.immediateSubmit([&](const CommandBuffer& commandBuffer) {
        flushTransfers(commandBuffer);
        transitionBuffer(commandBuffer, buffer, { .usage = BufferUsageType::TransferSrc, .access = AccessType::Read });
        const VkBufferCopy region{ .srcOffset = offset, .dstOffset = 0, .size = size };
        vkCmdCopyBuffer(commandBuffer.get(), buffer.buffer, readback.get(), 1, &region);
        device.emitMemoryBarrier(commandBuffer,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT);
    });
    readback.read(logicalDevice, out, size);
    device.destroyBuffer(readback);
}

void Engine::copyBuffer(BufferHandle source, BufferHandle destination, size_t size, size_t sourceOffset, size_t destinationOffset) {
    if (size == 0) return;
    const PersistentBuffer& from = buffers[source.id];
    const PersistentBuffer& to   = buffers[destination.id];
    if (sourceOffset + size > from.size)
        throw std::runtime_error(std::format("Copying {} bytes at offset {} overflows the buffer [{}] of {} bytes", size, sourceOffset, from.name, from.size));
    if (destinationOffset + size > to.size)
        throw std::runtime_error(std::format("Copying {} bytes at offset {} overflows the buffer [{}] of {} bytes", size, destinationOffset, to.name, to.size));

    pendingTransfers.push_back(PendingCopy{
        .image             = false,
        .source            = source.id,
        .destination       = destination.id,
        .sourceOffset      = sourceOffset,
        .destinationOffset = destinationOffset,
        .size              = size,
    });
}

// ——— Transfers ———————————————————————————————————————————————————————————————

void Engine::flush() {
    if (pendingTransfers.empty()) return;
    device.immediateSubmit([&](const CommandBuffer& commandBuffer) { flushTransfers(commandBuffer); });
}

void Engine::flushTransfers(const CommandBuffer& commandBuffer) {
    if (pendingTransfers.empty()) return;

    size_t stagingSize = 0;
    for (const PendingTransfer& transfer : pendingTransfers)
        if (const auto* write = std::get_if<PendingWrite>(&transfer)) stagingSize = alignStaging(stagingSize) + write->data.size();
    Buffer staging;
    if (stagingSize > 0)
        staging = device.createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingSize, BufferAllocMapped, nullptr, "EngineStaging");

    constexpr ImageAccessInfo imageSource{ .usage = ImageUsageType::TransferSrc, .access = AccessType::Read };
    constexpr ImageAccessInfo imageDestination{ .usage = ImageUsageType::TransferDst, .access = AccessType::Write };
    constexpr BufferAccessInfo bufferSource{ .usage = BufferUsageType::TransferSrc, .access = AccessType::Read };
    constexpr BufferAccessInfo bufferDestination{ .usage = BufferUsageType::TransferDst, .access = AccessType::Write };

    size_t stagingOffset = 0;
    for (const PendingTransfer& transfer : pendingTransfers) {
        if (const auto* write = std::get_if<PendingWrite>(&transfer)) {
            stagingOffset = alignStaging(stagingOffset);
            staging.write(logicalDevice, write->data.data(), write->data.size(), stagingOffset);

            if (write->image) {
                PersistentImage& image = images[write->destination];
                const ImageSubresourceRange subresources = resolveSubresources(image, write->subresources);
                const VkExtent2D extent = {image.width, image.height};
                const std::vector<VkBufferImageCopy> regions = bufferImageCopyRegions(
                    extent, subresources, stagingOffset, texelSize(image.name, extent, subresources, write->data.size())
                );
                transitionImage(commandBuffer, image, subresources, imageDestination);
                vkCmdCopyBufferToImage(
                    commandBuffer.get(), staging.get(), image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    static_cast<uint32_t>(regions.size()), regions.data()
                );
            } else {
                PersistentBuffer& buffer = buffers[write->destination];
                transitionBuffer(commandBuffer, buffer, bufferDestination);
                const VkBufferCopy region{ .srcOffset = stagingOffset, .dstOffset = write->offset, .size = write->data.size() };
                vkCmdCopyBuffer(commandBuffer.get(), staging.get(), buffer.buffer, 1, &region);
            }
            stagingOffset += write->data.size();
        } else {
            const PendingCopy& copy = std::get<PendingCopy>(transfer);
            if (copy.image) {
                PersistentImage& source      = images[copy.source];
                PersistentImage& destination = images[copy.destination];
                validateImageCopy(source, copy.sourceSubresources, destination, copy.destinationSubresources);
                const ImageSubresourceRange from = resolveSubresources(source, copy.sourceSubresources);
                const ImageSubresourceRange to = resolveSubresources(destination, copy.destinationSubresources);
                transitionImage(commandBuffer, source, from, imageSource);
                transitionImage(commandBuffer, destination, to, imageDestination);
                const std::vector<VkImageCopy> regions = imageCopyRegions({source.width, source.height}, from, to);
                vkCmdCopyImage(
                    commandBuffer.get(), source.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, destination.image,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(regions.size()), regions.data()
                );
            } else {
                PersistentBuffer& source      = buffers[copy.source];
                PersistentBuffer& destination = buffers[copy.destination];
                transitionBuffer(commandBuffer, source, bufferSource);
                transitionBuffer(commandBuffer, destination, bufferDestination);
                const VkBufferCopy region{ .srcOffset = copy.sourceOffset, .dstOffset = copy.destinationOffset, .size = copy.size };
                vkCmdCopyBuffer(commandBuffer.get(), source.buffer, destination.buffer, 1, &region);
            }
        }
    }

    pendingTransfers.clear();
    device.destroyBuffer(staging);
}

void Engine::dropPendingTransfers(bool image, uint32_t id) {
    std::erase_if(pendingTransfers, [&](const PendingTransfer& transfer) {
        if (const auto* write = std::get_if<PendingWrite>(&transfer))
            return write->image == image && write->destination == id;
        const PendingCopy& copy = std::get<PendingCopy>(transfer);
        return copy.image == image && (copy.source == id || copy.destination == id);
    });
}

void Engine::remapPendingTransfers(bool image, uint32_t a, uint32_t b) {
    const auto remap = [&](uint32_t& id) {
        if (id == a) id = b;
        else if (id == b) id = a;
    };
    for (PendingTransfer& transfer : pendingTransfers) {
        if (auto* write = std::get_if<PendingWrite>(&transfer)) {
            if (write->image == image) remap(write->destination);
        } else {
            PendingCopy& copy = std::get<PendingCopy>(transfer);
            if (copy.image == image) {
                remap(copy.source);
                remap(copy.destination);
            }
        }
    }
}

// ——— Storage —————————————————————————————————————————————————————————————————

ImageHandle Engine::insertImage(PersistentImage&& image) {
    if (freeImages.empty()) {
        images.push_back(std::move(image));
        return { .id = static_cast<uint32_t>(images.size() - 1) };
    }
    const uint32_t id = freeImages.back();
    freeImages.pop_back();
    images[id] = std::move(image);
    return { .id = id };
}

BufferHandle Engine::insertBuffer(PersistentBuffer&& buffer) {
    if (freeBuffers.empty()) {
        buffers.push_back(std::move(buffer));
        return { .id = static_cast<uint32_t>(buffers.size() - 1) };
    }
    const uint32_t id = freeBuffers.back();
    freeBuffers.pop_back();
    buffers[id] = std::move(buffer);
    return { .id = id };
}

void Engine::allocateImage(PersistentImage& image) {
    const VkImageUsageFlags usage = supportedImageUsage(device.getPhysicalDevice(), image.format);
    image.ownedImage = device.createImage(
        image.width, image.height, image.layout.mipCount, image.layout.layerCount, image.format, usage,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image.name
    );
    if (!image.ownedSampler.isValid()) image.ownedSampler = device.createSampler(image.name + "Sampler");

    image.image      = image.ownedImage.get();
    image.sampler    = image.ownedSampler.get();
    image.accesses.assign(image.layout.count(), ImageAccessInfo{});
}

void Engine::releaseImage(PersistentImage& image) {
    image.ownedViews.destroy(device);
    device.destroyImage(image.ownedImage);
    image.image = VK_NULL_HANDLE;
    image.view  = VK_NULL_HANDLE;
}

void Engine::allocateBuffer(PersistentBuffer& buffer) {
    buffer.ownedBuffer = device.createBuffer(kBufferUsage, buffer.size, 0, nullptr, buffer.name);
    buffer.buffer      = buffer.ownedBuffer.get();
    buffer.lastAccess  = std::nullopt;
}

void Engine::releaseBuffer(PersistentBuffer& buffer) {
    device.destroyBuffer(buffer.ownedBuffer);
    buffer.buffer = VK_NULL_HANDLE;
}

ImageSubresourceRange
Engine::resolveSubresources(const PersistentImage& image, ImageSubresourceRange subresources) const {
    const std::optional<ImageSubresourceRange> resolved = image.layout.resolve(subresources);
    if (!resolved)
        throw std::runtime_error(
            std::format(
                "The subresources [{}] are out of the image [{}] of {} mips and {} layers", subresources.toString(),
                image.name, image.layout.mipCount, image.layout.layerCount
            )
        );
    return *resolved;
}

void Engine::transitionImage(
    const CommandBuffer& commandBuffer, PersistentImage& image, ImageSubresourceRange subresources,
    ImageAccessInfo access
) {
    const std::vector<ImageTransition> transitions =
        transitionSubresources(image.accesses, image.layout, resolveSubresources(image, subresources), access);
    if (transitions.empty()) return;

    std::vector<VkImageMemoryBarrier2> barriers;
    for (const ImageTransition& transition : transitions) barriers.push_back(transition.toVk(image.image));
    const VkDependencyInfo dependency{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = static_cast<uint32_t>(barriers.size()),
        .pImageMemoryBarriers = barriers.data(),
    };
    logicalDevice.functions().cmdPipelineBarrier2(commandBuffer.get(), &dependency);
}

void Engine::transitionBuffer(const CommandBuffer& commandBuffer, PersistentBuffer& buffer, BufferAccessInfo access) {
    const VulkanBufferState dst = bufferAccessToVk(access);
    const VulkanBufferState src = buffer.lastAccess
        ? bufferAccessToVk(*buffer.lastAccess)
        : VulkanBufferState{ .access = VK_ACCESS_2_NONE, .stage = VK_PIPELINE_STAGE_2_NONE };
    const VkBufferMemoryBarrier2 barrier{
        .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .srcStageMask        = src.stage,
        .srcAccessMask       = src.access,
        .dstStageMask        = dst.stage,
        .dstAccessMask       = dst.access,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer              = buffer.buffer,
        .offset              = 0,
        .size                = VK_WHOLE_SIZE,
    };
    const VkDependencyInfo dependency{
        .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .bufferMemoryBarrierCount = 1,
        .pBufferMemoryBarriers    = &barrier,
    };
    logicalDevice.functions().cmdPipelineBarrier2(commandBuffer.get(), &dependency);
    buffer.lastAccess = access;
}

void Engine::beginSlot() {
    if (slotStarted) return;
    if (!slotFences[currentFrame].empty()) Fence::wait(slotFences[currentFrame], logicalDevice);
    device.collectRetired(currentFrame);
    slotStarted = true;
}

void Engine::registerFence(uint32_t slot, const Fence& fence) {
    if (std::ranges::find(slotFences[slot], &fence) == slotFences[slot].end())
        slotFences[slot].push_back(&fence);
    if (isHeadless()) slotStarted = false;
}

void Engine::unregisterFence(const Fence& fence) {
    for (std::vector<const Fence*>& fences : slotFences)
        std::erase(fences, &fence);
}

std::optional<uint32_t> Engine::acquireImage() {
    assert(swapchain);
    const std::optional<uint32_t> imageIndex = swapchain->acquire(imageAvailableSemaphores[currentFrame]);
    if (!imageIndex) {
        recreateSwapchain();
        return imageIndex;
    }

    PersistentImage& image = images[swapchainImage.id];
    image.image      = swapchain->getImage(*imageIndex).get();
    image.view       = swapchain->getImageView(*imageIndex).get();
    image.width      = swapchain->getExtent().width;
    image.height     = swapchain->getExtent().height;
    image.accesses.assign(image.layout.count(), ImageAccessInfo{});
    return imageIndex;
}

void Engine::presentImage(uint32_t imageIndex) {
    assert(swapchain);
    const bool outOfDate = swapchain->present(imageIndex, renderFinishedSemaphores[imageIndex]);
    if (outOfDate || framebufferResized) recreateSwapchain();
    currentFrame = (currentFrame + 1) % MAX_FRAME_IN_FLIGHT;
    slotStarted = false;
}

void Engine::recreateSwapchain() {
    assert(swapchain);

    window->pauseWhileMinimized();
    swapchain->recreate(window->getExtent());
    framebufferResized = false;

    if (renderFinishedSemaphores.size() != swapchain->getImageCount()) {
        device.waitIdle();
        destroyPresentSemaphores();
        createPresentSemaphores();
    }
}

void Engine::createPresentSemaphores() {
    imageAvailableSemaphores.resize(MAX_FRAME_IN_FLIGHT);
    for (Semaphore& semaphore : imageAvailableSemaphores)
        semaphore.init(logicalDevice);
    renderFinishedSemaphores.resize(swapchain->getImageCount());
    for (Semaphore& semaphore : renderFinishedSemaphores)
        semaphore.init(logicalDevice);
}

void Engine::destroyPresentSemaphores() {
    for (Semaphore& semaphore : imageAvailableSemaphores)
        semaphore.destroy(logicalDevice);
    imageAvailableSemaphores.clear();
    for (Semaphore& semaphore : renderFinishedSemaphores)
        semaphore.destroy(logicalDevice);
    renderFinishedSemaphores.clear();
}

}
