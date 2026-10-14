#pragma once

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>

#include "../graph/builder_resource.hpp"

namespace VkSmol {

class Buffer;
class CommandBuffer;
class ResourceRegistry;

inline constexpr ImageAccessInfo ImageTransferSource{.usage = ImageUsageType::TransferSrc, .access = AccessType::Read};
inline constexpr ImageAccessInfo ImageTransferDestination{
    .usage = ImageUsageType::TransferDst, .access = AccessType::Write
};
inline constexpr BufferAccessInfo BufferTransferSource{
    .usage = BufferUsageType::TransferSrc, .access = AccessType::Read
};
inline constexpr BufferAccessInfo BufferTransferDestination{
    .usage = BufferUsageType::TransferDst, .access = AccessType::Write
};

void checkBufferTransfer(const ResourceRegistry& resources, BufferHandle buffer, size_t offset, size_t size);
size_t checkImageTransfer(const ResourceRegistry& resources, ImageRange image, size_t size);
void checkBufferFill(const ResourceRegistry& resources, BufferHandle buffer, size_t offset, size_t size);
void checkBufferCopy(
    const ResourceRegistry& resources, BufferHandle source, size_t sourceOffset, BufferHandle destination,
    size_t destinationOffset, size_t size
);
void checkImageCopy(const ResourceRegistry& resources, ImageRange source, ImageRange destination);

class TransferCommands {
public:
    TransferCommands(ResourceRegistry& resources, const CommandBuffer& commandBuffer)
        : resources(resources), commandBuffer(commandBuffer) {}

    void fillBuffer(BufferHandle buffer, uint32_t value, size_t offset, size_t size);
    void clearImage(ImageRange image, VkClearColorValue color);

    void copyBuffer(
        BufferHandle source, BufferHandle destination, size_t size, size_t sourceOffset, size_t destinationOffset
    );
    void copyImage(ImageRange source, ImageRange destination);

    void
    uploadBuffer(const Buffer& staging, size_t stagingOffset, BufferHandle destination, size_t offset, size_t size);
    void uploadImage(const Buffer& staging, size_t stagingOffset, ImageRange destination, size_t size);

    void downloadBuffer(BufferHandle source, size_t offset, size_t size, const Buffer& readback);
    void downloadImage(ImageRange source, size_t size, const Buffer& readback);

private:
    ResourceRegistry& resources;
    const CommandBuffer& commandBuffer;
};

} // namespace VkSmol
