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
    imageAvailableSemaphores.resize(MAX_FRAME_IN_FLIGHT);
    for (Semaphore& semaphore : imageAvailableSemaphores)
        semaphore.init(logicalDevice);
    window->setResizeCallback([this](int, int) { framebufferResized = true; });
}

Engine::~Engine() {
    device.waitIdle();

    for (PersistentImage& image : images) {
        device.destroySampler(image.sampler);
        device.destroyImageView(image.view);
        device.destroyImage(image.image);
    }
    for (PersistentBuffer& buffer : buffers) device.destroyBuffer(buffer.buffer);

    if (window) {
        for (Semaphore& semaphore : imageAvailableSemaphores)
            semaphore.destroy(logicalDevice);
        window->setResizeCallback({});
    }
}

ImageHandle Engine::createImage(std::string name, VkFormat format, uint32_t width, uint32_t height) {
    constexpr VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
        | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    Image image = device.createImage(width, height, format, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, name);
    ImageView view = device.createImageView(image, name + "View");
    Sampler sampler = device.createSampler(name + "Sampler");

    PersistentImage entry = {
        .name       = std::move(name),
        .format     = format,
        .width      = width,
        .height     = height,
        .image      = std::move(image),
        .view       = std::move(view),
        .sampler    = std::move(sampler),
        .lastAccess = { .usage = ImageUsageType::Undefined, .access = AccessType::None },
    };

    if (freeImages.empty()) {
        images.push_back(std::move(entry));
        return { .id = static_cast<uint32_t>(images.size() - 1), .persistent = true };
    }
    const uint32_t id = freeImages.back();
    freeImages.pop_back();
    images[id] = std::move(entry);
    return { .id = id, .persistent = true };
}

void Engine::destroyImage(ImageHandle handle) {
    assert(handle.persistent);
    PersistentImage& image = images[handle.id];
    assert(image.image.get());
    device.destroySampler(image.sampler);
    device.destroyImageView(image.view);
    device.destroyImage(image.image);
    image = {};
    freeImages.push_back(handle.id);
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

void Engine::writeImage(ImageHandle handle, const void* data, size_t size) {
    assert(handle.persistent);
    PersistentImage& image = images[handle.id];

    Buffer staging = device.createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, size, BufferAllocMapped, const_cast<void*>(data), image.name + "Staging");
    device.immediateSubmit([&](const CommandBuffer& commandBuffer) {
        transitionImage(commandBuffer, image, { .usage = ImageUsageType::TransferDst, .access = AccessType::Write });
        image.image.fillFromBuffer(logicalDevice, commandBuffer, staging);
    });
    device.destroyBuffer(staging);
}

void Engine::readImage(ImageHandle handle, void* out, size_t size) {
    assert(handle.persistent);
    PersistentImage& image = images[handle.id];

    Buffer readback = device.createBuffer(VK_BUFFER_USAGE_TRANSFER_DST_BIT, size, BufferAllocMapped, nullptr, image.name + "Readback");
    device.immediateSubmit([&](const CommandBuffer& commandBuffer) {
        transitionImage(commandBuffer, image, { .usage = ImageUsageType::TransferSrc, .access = AccessType::Read });
        image.image.copyToBuffer(commandBuffer, readback);
        device.emitMemoryBarrier(commandBuffer,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT);
    });
    readback.read(logicalDevice, out, size);
    device.destroyBuffer(readback);
}

VkExtent2D Engine::getImageExtent(ImageHandle image) const {
    assert(image.persistent);
    return { images[image.id].width, images[image.id].height };
}

BufferHandle Engine::createBuffer(std::string name, size_t size, const void* initialData) {
    constexpr VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
        | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
        | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    Buffer buffer = device.createBuffer(usage, size, 0, nullptr, name);
    PersistentBuffer entry = { .name = std::move(name), .size = size, .buffer = std::move(buffer) };

    uint32_t id;
    if (freeBuffers.empty()) {
        id = static_cast<uint32_t>(buffers.size());
        buffers.push_back(std::move(entry));
    } else {
        id = freeBuffers.back();
        freeBuffers.pop_back();
        buffers[id] = std::move(entry);
    }

    const BufferHandle handle = { .id = id, .persistent = true };
    if (initialData) writeBuffer(handle, initialData, size);
    return handle;
}

void Engine::destroyBuffer(BufferHandle handle) {
    assert(handle.persistent);
    PersistentBuffer& buffer = buffers[handle.id];
    assert(buffer.buffer.get());
    device.destroyBuffer(buffer.buffer);
    buffer = {};
    freeBuffers.push_back(handle.id);
}

size_t Engine::getBufferSize(BufferHandle handle) const {
    assert(handle.persistent);
    return buffers[handle.id].size;
}

void Engine::writeBuffer(BufferHandle handle, const void* data, size_t size, size_t offset) {
    assert(handle.persistent);
    if (size == 0) return;
    PersistentBuffer& buffer = buffers[handle.id];
    if (offset + size > buffer.size)
        throw std::runtime_error(std::format("Writing {} bytes at offset {} overflows the buffer [{}] of {} bytes", size, offset, buffer.name, buffer.size));

    Buffer staging = device.createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, size, BufferAllocMapped, const_cast<void*>(data), buffer.name + "Staging");
    device.immediateSubmit([&](const CommandBuffer& commandBuffer) {
        transitionBuffer(commandBuffer, buffer, { .usage = BufferUsageType::TransferDst, .access = AccessType::Write });
        const VkBufferCopy region{ .srcOffset = 0, .dstOffset = offset, .size = size };
        vkCmdCopyBuffer(commandBuffer.get(), staging.get(), buffer.buffer.get(), 1, &region);
    });
    device.destroyBuffer(staging);
}

void Engine::readBuffer(BufferHandle handle, void* out, size_t size, size_t offset) {
    assert(handle.persistent);
    if (size == 0) return;
    PersistentBuffer& buffer = buffers[handle.id];
    if (offset + size > buffer.size)
        throw std::runtime_error(std::format("Reading {} bytes at offset {} overflows the buffer [{}] of {} bytes", size, offset, buffer.name, buffer.size));

    Buffer readback = device.createBuffer(VK_BUFFER_USAGE_TRANSFER_DST_BIT, size, BufferAllocMapped, nullptr, buffer.name + "Readback");
    device.immediateSubmit([&](const CommandBuffer& commandBuffer) {
        transitionBuffer(commandBuffer, buffer, { .usage = BufferUsageType::TransferSrc, .access = AccessType::Read });
        const VkBufferCopy region{ .srcOffset = offset, .dstOffset = 0, .size = size };
        vkCmdCopyBuffer(commandBuffer.get(), buffer.buffer.get(), readback.get(), 1, &region);
        device.emitMemoryBarrier(commandBuffer,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT);
    });
    readback.read(logicalDevice, out, size);
    device.destroyBuffer(readback);
}

void Engine::transitionImage(const CommandBuffer& commandBuffer, PersistentImage& image, ImageAccessInfo access) {
    const VulkanImageState src = imageAccessToVk(image.lastAccess);
    const VulkanImageState dst = imageAccessToVk(access);
    ImageBarrier barrier;
    barrier.call(
        logicalDevice, commandBuffer, image.image.get(),
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
        .buffer              = buffer.buffer.get(),
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
    if (!imageIndex) recreateSwapchain();
    return imageIndex;
}

void Engine::presentImage(uint32_t imageIndex, const Semaphore& waitSemaphore) {
    assert(swapchain);
    const bool outOfDate = swapchain->present(imageIndex, waitSemaphore);
    if (outOfDate || framebufferResized) recreateSwapchain();
    currentFrame = (currentFrame + 1) % MAX_FRAME_IN_FLIGHT;
    slotStarted = false;
}

void Engine::recreateSwapchain() {
    assert(swapchain);

    window->pauseWhileMinimized();
    swapchain->recreate(window->getExtent());
    framebufferResized = false;
}

}
