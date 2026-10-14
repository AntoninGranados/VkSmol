#include "pending_transfers.hpp"

#include <type_traits>

#include "../device/device.hpp"
#include "../memory/buffer.hpp"
#include "../resource/resource_registry.hpp"
#include "transfer_commands.hpp"

namespace VkSmol {

namespace {

constexpr size_t kStagingAlignment = 16;

size_t alignStaging(size_t offset) { return (offset + kStagingAlignment - 1) & ~(kStagingAlignment - 1); }

template <class Handle> void swapHandle(Handle& handle, Handle a, Handle b) {
    if (handle == a)
        handle = b;
    else if (handle == b)
        handle = a;
}

} // namespace

void PendingTransfers::writeBuffer(BufferHandle buffer, size_t offset, const void* data, size_t size) {
    if (size == 0) return;
    checkBufferTransfer(resources, buffer, offset, size);
    const std::byte* bytes = static_cast<const std::byte*>(data);
    transfers.push_back(BufferWrite{.destination = buffer, .offset = offset, .data = {bytes, bytes + size}});
}

void PendingTransfers::writeImage(ImageRange image, const void* data, size_t size) {
    if (size == 0) return;
    checkImageTransfer(resources, image, size);
    const std::byte* bytes = static_cast<const std::byte*>(data);
    transfers.push_back(ImageWrite{.destination = image, .data = {bytes, bytes + size}});
}

void PendingTransfers::copyBuffer(
    BufferHandle source, BufferHandle destination, size_t size, size_t sourceOffset, size_t destinationOffset
) {
    if (size == 0) return;
    checkBufferCopy(resources, source, sourceOffset, destination, destinationOffset, size);
    transfers.push_back(
        BufferCopy{
            .source = source,
            .destination = destination,
            .size = size,
            .sourceOffset = sourceOffset,
            .destinationOffset = destinationOffset,
        }
    );
}

void PendingTransfers::copyImage(ImageRange source, ImageRange destination) {
    checkImageCopy(resources, source, destination);
    transfers.push_back(ImageCopy{.source = source, .destination = destination});
}

void PendingTransfers::flush(const CommandBuffer& commandBuffer) {
    if (transfers.empty()) return;

    size_t stagingSize = 0;
    for (const Transfer& transfer : transfers) {
        if (const auto* write = std::get_if<BufferWrite>(&transfer))
            stagingSize = alignStaging(stagingSize) + write->data.size();
        else if (const auto* write = std::get_if<ImageWrite>(&transfer))
            stagingSize = alignStaging(stagingSize) + write->data.size();
    }
    Buffer staging;
    if (stagingSize > 0)
        staging = device.createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingSize, BufferAllocMapped, "Staging");

    TransferCommands commands(resources, commandBuffer);
    size_t stagingOffset = 0;
    const auto stage = [&](const std::vector<std::byte>& data) {
        stagingOffset = alignStaging(stagingOffset);
        staging.write(device.getLogicalDevice(), data.data(), data.size(), stagingOffset);
        const size_t offset = stagingOffset;
        stagingOffset += data.size();
        return offset;
    };

    for (const Transfer& transfer : transfers) {
        std::visit(
            [&](const auto& pending) {
                using T = std::decay_t<decltype(pending)>;
                if constexpr (std::is_same_v<T, BufferWrite>) {
                    const size_t offset = stage(pending.data);
                    resources.transitionBuffer(commandBuffer, pending.destination, BufferTransferDestination);
                    commands.uploadBuffer(staging, offset, pending.destination, pending.offset, pending.data.size());
                } else if constexpr (std::is_same_v<T, ImageWrite>) {
                    const size_t offset = stage(pending.data);
                    resources.transitionImage(commandBuffer, pending.destination, ImageTransferDestination);
                    commands.uploadImage(staging, offset, pending.destination, pending.data.size());
                } else if constexpr (std::is_same_v<T, BufferCopy>) {
                    resources.transitionBuffer(commandBuffer, pending.source, BufferTransferSource);
                    resources.transitionBuffer(commandBuffer, pending.destination, BufferTransferDestination);
                    commands.copyBuffer(
                        pending.source, pending.destination, pending.size, pending.sourceOffset,
                        pending.destinationOffset
                    );
                } else {
                    resources.transitionImage(commandBuffer, pending.source, ImageTransferSource);
                    resources.transitionImage(commandBuffer, pending.destination, ImageTransferDestination);
                    commands.copyImage(pending.source, pending.destination);
                }
            },
            transfer
        );
    }

    transfers.clear();
    device.destroyBuffer(staging);
}

void PendingTransfers::drop(ImageHandle image) {
    std::erase_if(transfers, [&](const Transfer& transfer) {
        if (const auto* write = std::get_if<ImageWrite>(&transfer)) return write->destination.image == image;
        if (const auto* copy = std::get_if<ImageCopy>(&transfer))
            return copy->source.image == image || copy->destination.image == image;
        return false;
    });
}

void PendingTransfers::drop(BufferHandle buffer) {
    std::erase_if(transfers, [&](const Transfer& transfer) {
        if (const auto* write = std::get_if<BufferWrite>(&transfer)) return write->destination == buffer;
        if (const auto* copy = std::get_if<BufferCopy>(&transfer))
            return copy->source == buffer || copy->destination == buffer;
        return false;
    });
}

void PendingTransfers::remap(ImageHandle a, ImageHandle b) {
    for (Transfer& transfer : transfers) {
        if (auto* write = std::get_if<ImageWrite>(&transfer)) swapHandle(write->destination.image, a, b);
        if (auto* copy = std::get_if<ImageCopy>(&transfer)) {
            swapHandle(copy->source.image, a, b);
            swapHandle(copy->destination.image, a, b);
        }
    }
}

void PendingTransfers::remap(BufferHandle a, BufferHandle b) {
    for (Transfer& transfer : transfers) {
        if (auto* write = std::get_if<BufferWrite>(&transfer)) swapHandle(write->destination, a, b);
        if (auto* copy = std::get_if<BufferCopy>(&transfer)) {
            swapHandle(copy->source, a, b);
            swapHandle(copy->destination, a, b);
        }
    }
}

} // namespace VkSmol
