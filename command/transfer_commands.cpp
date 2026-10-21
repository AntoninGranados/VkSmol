#include "transfer_commands.hpp"

#include <format>
#include <stdexcept>
#include <vector>

#include "../image/image_format.hpp"
#include "../memory/buffer.hpp"
#include "../resource/resource_registry.hpp"
#include "command_buffer.hpp"

namespace VkSmol {

namespace {

VkImageAspectFlags transferAspect(const ImageResource& image) {
    const VkImageAspectFlags aspects = formatAspects(image.format);
    if (aspects == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
        throw std::runtime_error(
            std::format(
                "Cannot transfer the combined depth and stencil image [{}] through a buffer, use separate formats",
                image.name
            )
        );
    return aspects;
}

} // namespace

// ——— Validation ——————————————————————————————————————————————————————————————

void checkBufferTransfer(const ResourceRegistry& resources, BufferHandle handle, size_t offset, size_t size) {
    const BufferResource& buffer = resources.getBuffer(handle);
    if (offset > buffer.size || size > buffer.size - offset)
        throw std::runtime_error(
            std::format(
                "Transferring {} bytes at offset {} overflows the buffer [{}] of {} bytes", size, offset, buffer.name,
                buffer.size
            )
        );
}

size_t checkImageTransfer(const ResourceRegistry& resources, ImageRange image, size_t size) {
    const size_t texels = texelCount(resources.getImage(image.image).getExtent(), resources.resolve(image));
    if (size % texels != 0)
        throw std::runtime_error(
            std::format(
                "{} bytes do not tightly pack the {} texels of [{}] in the image [{}]", size, texels,
                resources.resolve(image).toString(), resources.getImage(image.image).name
            )
        );
    return size / texels;
}

void checkBufferFill(const ResourceRegistry& resources, BufferHandle handle, size_t offset, size_t size) {
    const BufferResource& buffer = resources.getBuffer(handle);
    const bool whole = size == VK_WHOLE_SIZE;
    if (offset % 4 != 0 || (!whole && size % 4 != 0))
        throw std::runtime_error(
            std::format(
                "Filling the buffer [{}] needs a 4-byte aligned offset and size, got offset {} and size {}",
                buffer.name, offset, size
            )
        );
    if (offset >= buffer.size || (!whole && size > buffer.size - offset))
        throw std::runtime_error(
            std::format(
                "Filling {} bytes at offset {} overflows the buffer [{}] of {} bytes",
                whole ? buffer.size - offset : size, offset, buffer.name, buffer.size
            )
        );
}

void checkBufferCopy(
    const ResourceRegistry& resources, BufferHandle source, size_t sourceOffset, BufferHandle destination,
    size_t destinationOffset, size_t size
) {
    checkBufferTransfer(resources, source, sourceOffset, size);
    checkBufferTransfer(resources, destination, destinationOffset, size);
    if (source == destination && sourceOffset < destinationOffset + size && destinationOffset < sourceOffset + size)
        throw std::runtime_error(
            std::format("Cannot copy overlapping bytes of the buffer [{}]", resources.getBuffer(source).name)
        );
}

void checkImageCopy(const ResourceRegistry& resources, ImageRange source, ImageRange destination) {
    const ImageResource& sourceImage = resources.getImage(source.image);
    const ImageResource& destinationImage = resources.getImage(destination.image);
    const ImageSubresourceRange from = resources.resolve(source);
    const ImageSubresourceRange to = resources.resolve(destination);
    if (from.mipCount != to.mipCount || from.layerCount != to.layerCount)
        throw std::runtime_error(
            std::format(
                "Cannot copy {} mips and {} layers of the image [{}] into {} mips and {} layers of the image [{}]",
                from.mipCount, from.layerCount, sourceImage.name, to.mipCount, to.layerCount, destinationImage.name
            )
        );
    if (formatAspects(sourceImage.format) != formatAspects(destinationImage.format))
        throw std::runtime_error(
            std::format(
                "Cannot copy the image [{}] into the image [{}] as their formats have different aspects",
                sourceImage.name, destinationImage.name
            )
        );
    if (source.image == destination.image && from.overlaps(to))
        throw std::runtime_error(
            std::format("Cannot copy overlapping subresources of the image [{}]", sourceImage.name)
        );

    for (uint32_t i = 0; i < from.mipCount; ++i) {
        const VkExtent2D fromExtent = mipExtent(sourceImage.getExtent(), from.baseMip + i);
        const VkExtent2D toExtent = mipExtent(destinationImage.getExtent(), to.baseMip + i);
        if (fromExtent.width != toExtent.width || fromExtent.height != toExtent.height)
            throw std::runtime_error(
                std::format(
                    "Cannot copy mip {} of the image [{}] of {}x{} into mip {} of the image [{}] of {}x{}",
                    from.baseMip + i, sourceImage.name, fromExtent.width, fromExtent.height, to.baseMip + i,
                    destinationImage.name, toExtent.width, toExtent.height
                )
            );
    }
}

// ——— Recording ———————————————————————————————————————————————————————————————

void TransferCommands::fillBuffer(BufferHandle buffer, uint32_t value, size_t offset, size_t size) {
    checkBufferFill(resources, buffer, offset, size);
    vkCmdFillBuffer(commandBuffer.get(), resources.getBuffer(buffer).buffer, offset, size, value);
}

void TransferCommands::clearImage(ImageRange image, VkClearColorValue color) {
    const ImageResource& resource = resources.getImage(image.image);
    if (formatAspects(resource.format) != VK_IMAGE_ASPECT_COLOR_BIT)
        throw std::runtime_error(
            std::format("Cannot clear the depth or stencil image [{}] with a color", resource.name)
        );
    const VkImageSubresourceRange range = resources.resolve(image).toVk(VK_IMAGE_ASPECT_COLOR_BIT);
    vkCmdClearColorImage(commandBuffer.get(), resource.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1, &range);
}

void TransferCommands::clearImage(ImageRange image, VkClearDepthStencilValue value) {
    const ImageResource& resource = resources.getImage(image.image);
    const VkImageAspectFlags aspects = formatAspects(resource.format);
    if (aspects == VK_IMAGE_ASPECT_COLOR_BIT)
        throw std::runtime_error(
            std::format("Cannot clear the color image [{}] with a depth and stencil value", resource.name)
        );
    const VkImageSubresourceRange range = resources.resolve(image).toVk(aspects);
    vkCmdClearDepthStencilImage(
        commandBuffer.get(), resource.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &value, 1, &range
    );
}

void TransferCommands::copyBuffer(
    BufferHandle source, BufferHandle destination, size_t size, size_t sourceOffset, size_t destinationOffset
) {
    if (size == 0) return;
    checkBufferCopy(resources, source, sourceOffset, destination, destinationOffset, size);
    const VkBufferCopy region{.srcOffset = sourceOffset, .dstOffset = destinationOffset, .size = size};
    vkCmdCopyBuffer(
        commandBuffer.get(), resources.getBuffer(source).buffer, resources.getBuffer(destination).buffer, 1, &region
    );
}

void TransferCommands::copyImage(ImageRange source, ImageRange destination) {
    checkImageCopy(resources, source, destination);
    const ImageResource& sourceImage = resources.getImage(source.image);
    const std::vector<VkImageCopy> regions = imageCopyRegions(
        sourceImage.getExtent(), resources.resolve(source), resources.resolve(destination),
        formatAspects(sourceImage.format)
    );
    vkCmdCopyImage(
        commandBuffer.get(), resources.getImage(source.image).image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        resources.getImage(destination.image).image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        static_cast<uint32_t>(regions.size()), regions.data()
    );
}

void TransferCommands::uploadBuffer(
    const Buffer& staging, size_t stagingOffset, BufferHandle destination, size_t offset, size_t size
) {
    checkBufferTransfer(resources, destination, offset, size);
    const VkBufferCopy region{.srcOffset = stagingOffset, .dstOffset = offset, .size = size};
    vkCmdCopyBuffer(commandBuffer.get(), staging.get(), resources.getBuffer(destination).buffer, 1, &region);
}

void TransferCommands::uploadImage(const Buffer& staging, size_t stagingOffset, ImageRange destination, size_t size) {
    const size_t texelSize = checkImageTransfer(resources, destination, size);
    const ImageResource& image = resources.getImage(destination.image);
    const std::vector<VkBufferImageCopy> regions = bufferImageCopyRegions(
        image.getExtent(), resources.resolve(destination), transferAspect(image), stagingOffset, texelSize
    );
    vkCmdCopyBufferToImage(
        commandBuffer.get(), staging.get(), image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        static_cast<uint32_t>(regions.size()), regions.data()
    );
}

void TransferCommands::downloadBuffer(BufferHandle source, size_t offset, size_t size, const Buffer& readback) {
    checkBufferTransfer(resources, source, offset, size);
    const VkBufferCopy region{.srcOffset = offset, .dstOffset = 0, .size = size};
    vkCmdCopyBuffer(commandBuffer.get(), resources.getBuffer(source).buffer, readback.get(), 1, &region);
}

void TransferCommands::downloadImage(ImageRange source, size_t size, const Buffer& readback) {
    const size_t texelSize = checkImageTransfer(resources, source, size);
    const ImageResource& image = resources.getImage(source.image);
    const std::vector<VkBufferImageCopy> regions =
        bufferImageCopyRegions(image.getExtent(), resources.resolve(source), transferAspect(image), 0, texelSize);
    vkCmdCopyImageToBuffer(
        commandBuffer.get(), image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback.get(),
        static_cast<uint32_t>(regions.size()), regions.data()
    );
}

} // namespace VkSmol
