#include "engine.hpp"

#include <algorithm>
#include <cassert>
#include <format>
#include <stdexcept>

#include <stb_image/stb_image.h>

#include "image/image_barrier.hpp"
#include "graph/compiled_resource.hpp"
#include "graph/graph_utils.hpp"

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

}

// ——— Images ——————————————————————————————————————————————————————————————————

ImageHandle Engine::createImage(std::string name, VkFormat format, uint32_t width, uint32_t height) {
    PersistentImage image = { .name = std::move(name), .format = format, .width = width, .height = height };
    allocateImage(image);
    return insertImage(std::move(image));
}

ImageHandle Engine::importImage(std::string name, VkImage handle, VkImageView view, VkFormat format, uint32_t width, uint32_t height,
                                ImageAccessInfo currentAccess) {
    PersistentImage image = {
        .name       = std::move(name),
        .format     = format,
        .width      = width,
        .height     = height,
        .image      = handle,
        .view       = view,
        .lastAccess = currentAccess,
        .imported   = true,
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
    dropPendingWrites(true, handle.id);
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

    dropPendingWrites(true, handle.id);
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

    for (PendingWrite& write : pendingWrites) {
        if (!write.image) continue;
        if (write.id == a.id) write.id = b.id;
        else if (write.id == b.id) write.id = a.id;
    }
}

const std::string& Engine::getImageName(ImageHandle handle) const {
    return images[handle.id].name;
}

VkExtent2D Engine::getImageExtent(ImageHandle handle) const {
    return { images[handle.id].width, images[handle.id].height };
}

VkFormat Engine::getImageFormat(ImageHandle handle) const {
    return images[handle.id].format;
}

VkImage Engine::getImage(ImageHandle handle) const {
    return images[handle.id].image;
}

VkImageView Engine::getImageView(ImageHandle handle) const {
    return images[handle.id].view;
}

VkSampler Engine::getSampler(ImageHandle handle) const {
    return images[handle.id].sampler;
}

void Engine::writeImage(ImageHandle handle, const void* data, size_t size) {
    if (size == 0) return;
    const std::byte* bytes = static_cast<const std::byte*>(data);
    pendingWrites.push_back({ .image = true, .id = handle.id, .offset = 0, .data = std::vector<std::byte>(bytes, bytes + size) });
}

void Engine::readImage(ImageHandle handle, void* out, size_t size) {
    if (size == 0) return;
    PersistentImage& image = images[handle.id];

    Buffer readback = device.createBuffer(VK_BUFFER_USAGE_TRANSFER_DST_BIT, size, BufferAllocMapped, nullptr, image.name + "Readback");
    device.immediateSubmit([&](const CommandBuffer& commandBuffer) {
        flushWrites(commandBuffer);
        transitionImage(commandBuffer, image, { .usage = ImageUsageType::TransferSrc, .access = AccessType::Read });
        const VkBufferImageCopy region{
            .imageSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1 },
            .imageExtent      = { image.width, image.height, 1 },
        };
        vkCmdCopyImageToBuffer(commandBuffer.get(), image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback.get(), 1, &region);
        device.emitMemoryBarrier(commandBuffer,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT);
    });
    readback.read(logicalDevice, out, size);
    device.destroyBuffer(readback);
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
    dropPendingWrites(false, handle.id);
    releaseBuffer(buffer);
    buffer = {};
    freeBuffers.push_back(handle.id);
}

void Engine::resizeBuffer(BufferHandle handle, size_t size) {
    PersistentBuffer& buffer = buffers[handle.id];
    if (buffer.imported)
        throw std::runtime_error(std::format("Cannot resize the buffer [{}] as it is imported", buffer.name));
    if (buffer.size == size) return;

    dropPendingWrites(false, handle.id);
    releaseBuffer(buffer);
    buffer.size = size;
    allocateBuffer(buffer);
}

void Engine::swapBuffers(BufferHandle a, BufferHandle b) {
    PersistentBuffer& first  = buffers[a.id];
    PersistentBuffer& second = buffers[b.id];
    std::swap(first.name, second.name);
    std::swap(first, second);

    for (PendingWrite& write : pendingWrites) {
        if (write.image) continue;
        if (write.id == a.id) write.id = b.id;
        else if (write.id == b.id) write.id = a.id;
    }
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
    pendingWrites.push_back({ .image = false, .id = handle.id, .offset = offset, .data = std::vector<std::byte>(bytes, bytes + size) });
}

void Engine::readBuffer(BufferHandle handle, void* out, size_t size, size_t offset) {
    if (size == 0) return;
    PersistentBuffer& buffer = buffers[handle.id];
    if (offset + size > buffer.size)
        throw std::runtime_error(std::format("Reading {} bytes at offset {} overflows the buffer [{}] of {} bytes", size, offset, buffer.name, buffer.size));

    Buffer readback = device.createBuffer(VK_BUFFER_USAGE_TRANSFER_DST_BIT, size, BufferAllocMapped, nullptr, buffer.name + "Readback");
    device.immediateSubmit([&](const CommandBuffer& commandBuffer) {
        flushWrites(commandBuffer);
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

// ——— Transfers ———————————————————————————————————————————————————————————————

void Engine::flush() {
    if (pendingWrites.empty()) return;
    device.immediateSubmit([&](const CommandBuffer& commandBuffer) { flushWrites(commandBuffer); });
}

void Engine::flushWrites(const CommandBuffer& commandBuffer) {
    if (pendingWrites.empty()) return;

    size_t stagingSize = 0;
    for (const PendingWrite& write : pendingWrites) stagingSize = alignStaging(stagingSize) + write.data.size();
    Buffer staging = device.createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingSize, BufferAllocMapped, nullptr, "EngineStaging");

    size_t stagingOffset = 0;
    for (const PendingWrite& write : pendingWrites) {
        stagingOffset = alignStaging(stagingOffset);
        staging.write(logicalDevice, write.data.data(), write.data.size(), stagingOffset);

        if (write.image) {
            PersistentImage& image = images[write.id];
            transitionImage(commandBuffer, image, { .usage = ImageUsageType::TransferDst, .access = AccessType::Write });
            const VkBufferImageCopy region{
                .bufferOffset     = stagingOffset,
                .imageSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1 },
                .imageExtent      = { image.width, image.height, 1 },
            };
            vkCmdCopyBufferToImage(commandBuffer.get(), staging.get(), image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        } else {
            PersistentBuffer& buffer = buffers[write.id];
            transitionBuffer(commandBuffer, buffer, { .usage = BufferUsageType::TransferDst, .access = AccessType::Write });
            const VkBufferCopy region{ .srcOffset = stagingOffset, .dstOffset = write.offset, .size = write.data.size() };
            vkCmdCopyBuffer(commandBuffer.get(), staging.get(), buffer.buffer, 1, &region);
        }
        stagingOffset += write.data.size();
    }

    pendingWrites.clear();
    device.destroyBuffer(staging);
}

void Engine::dropPendingWrites(bool image, uint32_t id) {
    std::erase_if(pendingWrites, [&](const PendingWrite& write) { return write.image == image && write.id == id; });
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
    image.ownedImage = device.createImage(image.width, image.height, image.format, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image.name);
    image.ownedView  = device.createImageView(image.ownedImage, image.name + "View");
    if (!image.ownedSampler.isValid()) image.ownedSampler = device.createSampler(image.name + "Sampler");

    image.image      = image.ownedImage.get();
    image.view       = image.ownedView.get();
    image.sampler    = image.ownedSampler.get();
    image.lastAccess = { .usage = ImageUsageType::Undefined, .access = AccessType::None };
}

void Engine::releaseImage(PersistentImage& image) {
    device.destroyImageView(image.ownedView);
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

void Engine::transitionImage(const CommandBuffer& commandBuffer, PersistentImage& image, ImageAccessInfo access) {
    const VulkanImageState src = imageAccessToVk(image.lastAccess);
    const VulkanImageState dst = imageAccessToVk(access);
    ImageBarrier barrier;
    barrier.call(
        logicalDevice, commandBuffer, image.image,
        src.layout, dst.layout,
        src.access, dst.access,
        src.stage, dst.stage,
        dst.aspect
    );
    image.lastAccess = access;
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
    image.lastAccess = { .usage = ImageUsageType::Undefined, .access = AccessType::None };
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
