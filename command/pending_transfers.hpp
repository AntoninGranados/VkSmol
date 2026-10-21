#pragma once

#include <cstddef>
#include <variant>
#include <vector>

#include "../graph/builder_resource.hpp"

namespace VkSmol {

class CommandBuffer;
class Device;
class ResourceRegistry;

class PendingTransfers {
public:
    PendingTransfers(Device& device, ResourceRegistry& resources) : device(device), resources(resources) {}

    void writeBuffer(BufferHandle buffer, size_t offset, const void* data, size_t size);
    void writeImage(ImageRange image, const void* data, size_t size);
    void copyBuffer(
        BufferHandle source, BufferHandle destination, size_t size, size_t sourceOffset, size_t destinationOffset
    );
    void copyImage(ImageRange source, ImageRange destination);

    bool empty() const { return transfers.empty(); }
    void flush(const CommandBuffer& commandBuffer);

    void drop(ImageHandle image);
    void drop(BufferHandle buffer);
    void remap(ImageHandle a, ImageHandle b);
    void remap(BufferHandle a, BufferHandle b);

private:
    struct BufferWrite {
        BufferHandle destination;
        size_t offset;
        std::vector<std::byte> data;
    };

    struct ImageWrite {
        ImageRange destination;
        std::vector<std::byte> data;
    };

    struct BufferCopy {
        BufferHandle source;
        BufferHandle destination;
        size_t size;
        size_t sourceOffset;
        size_t destinationOffset;
    };

    struct ImageCopy {
        ImageRange source;
        ImageRange destination;
    };

    using Transfer = std::variant<BufferWrite, ImageWrite, BufferCopy, ImageCopy>;

    Device& device;
    ResourceRegistry& resources;
    std::vector<Transfer> transfers;
};

} // namespace VkSmol
