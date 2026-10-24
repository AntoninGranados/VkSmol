#include "engine.hpp"

#include <algorithm>
#include <cassert>
#include <format>
#include <stdexcept>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>

#include "command/transfer_commands.hpp"

namespace VkSmol {

Engine::Engine(const std::string& appName, uint32_t appVersion, std::optional<Window&> window, EngineFeatures features)
    : window(window), device(appName, appVersion, window, features), logicalDevice(device.getLogicalDevice()),
      resources(device), pendingTransfers(device, resources) {
    if (!window) return;

    swapchain.emplace(device, window->getExtent());
    swapchainImage = importImage("Swapchain", VK_NULL_HANDLE, VK_NULL_HANDLE, swapchain->getImageFormat(),
                                 swapchain->getExtent().width, swapchain->getExtent().height);
    createPresentSemaphores();
    window->setResizeCallback([this](int, int) { framebufferResized = true; });
}

Engine::~Engine() {
    device.waitIdle();

    if (window) {
        destroyPresentSemaphores();
        window->setResizeCallback({});
    }
}

// ——— Images ——————————————————————————————————————————————————————————————————

ImageHandle Engine::createImage(
    std::string name, VkFormat format, uint32_t width, uint32_t height, uint32_t layers, uint32_t mips
) {
    return resources.createImage(std::move(name), format, width, height, layers, mips);
}

ImageHandle Engine::importImage(std::string name, VkImage handle, VkImageView view, VkFormat format, uint32_t width, uint32_t height,
                                ImageAccessInfo currentAccess) {
    return resources.importImage(std::move(name), handle, view, format, width, height, currentAccess);
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

void Engine::destroyImage(ImageHandle image) {
    pendingTransfers.drop(image);
    resources.destroyImage(image);
}

void Engine::resizeImage(ImageHandle image, uint32_t width, uint32_t height) {
    if (resources.resizeImage(image, width, height)) pendingTransfers.drop(image);
}

void Engine::swapImages(ImageHandle a, ImageHandle b) {
    resources.swapImages(a, b);
    pendingTransfers.remap(a, b);
}

const std::string& Engine::getImageName(ImageHandle image) const { return resources.getImage(image).name; }

VkExtent2D Engine::getImageExtent(ImageHandle handle, uint32_t mip) const {
    const ImageResource& image = resources.getImage(handle);
    if (mip >= image.layout.mipCount)
        throw std::runtime_error(
            std::format("The image [{}] has no mip {} (it has {})", image.name, mip, image.layout.mipCount)
        );
    return mipExtent(image.getExtent(), mip);
}

uint32_t Engine::getImageLayerCount(ImageHandle image) const { return resources.getImage(image).layout.layerCount; }

uint32_t Engine::getImageMipCount(ImageHandle image) const { return resources.getImage(image).layout.mipCount; }

VkFormat Engine::getImageFormat(ImageHandle image) const { return resources.getImage(image).format; }

VkImage Engine::getImage(ImageHandle image) const { return resources.getImage(image).image; }

VkImageView Engine::getImageView(ImageRange image, std::optional<VkImageViewType> type) {
    return resources.getImageView(image, type);
}

VkSampler Engine::getSampler(ImageHandle image) const { return resources.getImage(image).ownedSampler.get(); }

ImageSubresourceRange Engine::resolveImageRange(ImageRange image) const { return resources.resolve(image); }

void Engine::writeImage(ImageRange image, const void* data, size_t size) {
    pendingTransfers.writeImage(image, data, size);
}

void Engine::readImage(ImageRange image, void* out, size_t size) {
    if (size == 0) return;
    checkImageTransfer(resources, image, size);

    Buffer readback = device.createBuffer(VK_BUFFER_USAGE_TRANSFER_DST_BIT, size, BufferAllocMapped);
    device.immediateSubmit([&](const CommandBuffer& commandBuffer) {
        pendingTransfers.flush(commandBuffer);
        resources.transitionImage(commandBuffer, image, ImageTransferSource);
        TransferCommands(resources, commandBuffer).downloadImage(image, size, readback);
        device.emitMemoryBarrier(commandBuffer,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT);
    });
    readback.read(logicalDevice, out, size);
    device.destroyBuffer(readback);
}

void Engine::copyImage(ImageRange source, ImageRange destination) { pendingTransfers.copyImage(source, destination); }

// ——— Buffers —————————————————————————————————————————————————————————————————

BufferHandle Engine::createBuffer(std::string name, size_t size, const void* initialData) {
    const BufferHandle buffer = resources.createBuffer(std::move(name), size);
    if (initialData) writeBuffer(buffer, initialData, size);
    return buffer;
}

BufferHandle Engine::importBuffer(std::string name, VkBuffer handle, size_t size) {
    return resources.importBuffer(std::move(name), handle, size);
}

void Engine::destroyBuffer(BufferHandle buffer) {
    pendingTransfers.drop(buffer);
    resources.destroyBuffer(buffer);
}

void Engine::resizeBuffer(BufferHandle buffer, size_t size) {
    if (resources.resizeBuffer(buffer, size)) pendingTransfers.drop(buffer);
}

void Engine::swapBuffers(BufferHandle a, BufferHandle b) {
    resources.swapBuffers(a, b);
    pendingTransfers.remap(a, b);
}

const std::string& Engine::getBufferName(BufferHandle buffer) const { return resources.getBuffer(buffer).name; }

size_t Engine::getBufferSize(BufferHandle buffer) const { return resources.getBuffer(buffer).size; }

VkBuffer Engine::getBuffer(BufferHandle buffer) const { return resources.getBuffer(buffer).buffer; }

void Engine::writeBuffer(BufferHandle buffer, const void* data, size_t size, size_t offset) {
    pendingTransfers.writeBuffer(buffer, offset, data, size);
}

void Engine::readBuffer(BufferHandle buffer, void* out, size_t size, size_t offset) {
    if (size == 0) return;
    checkBufferTransfer(resources, buffer, offset, size);

    Buffer readback = device.createBuffer(VK_BUFFER_USAGE_TRANSFER_DST_BIT, size, BufferAllocMapped);
    device.immediateSubmit([&](const CommandBuffer& commandBuffer) {
        pendingTransfers.flush(commandBuffer);
        resources.transitionBuffer(commandBuffer, buffer, BufferTransferSource);
        TransferCommands(resources, commandBuffer).downloadBuffer(buffer, offset, size, readback);
        device.emitMemoryBarrier(commandBuffer,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT);
    });
    readback.read(logicalDevice, out, size);
    device.destroyBuffer(readback);
}

void Engine::copyBuffer(BufferHandle source, BufferHandle destination, size_t size, size_t sourceOffset, size_t destinationOffset) {
    pendingTransfers.copyBuffer(source, destination, size, sourceOffset, destinationOffset);
}

// ——— Transfers ———————————————————————————————————————————————————————————————

void Engine::flush() {
    if (pendingTransfers.empty()) return;
    device.immediateSubmit([&](const CommandBuffer& commandBuffer) { pendingTransfers.flush(commandBuffer); });
}

// ——— Frames ——————————————————————————————————————————————————————————————————

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

    ImageResource& image = resources.getImage(swapchainImage);
    image.image = swapchain->getImage(*imageIndex).get();
    image.view = swapchain->getImageView(*imageIndex).get();
    image.width = swapchain->getExtent().width;
    image.height = swapchain->getExtent().height;
    image.state.reset(image.layout);
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
