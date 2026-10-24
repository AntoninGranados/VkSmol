#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <optional>
#include <type_traits>

#include "builder_resource.hpp"

namespace VkSmol {

class RenderGraph;
class CommandBuffer;
class ResourceRegistry;

class PassContext {
public:
    const CommandBuffer& getCommandBuffer() const { return commandBuffer; }
    VkCommandBuffer cmd() const;
    VkExtent2D getExtent() const { return extent; }

    void dispatch(uint32_t x, uint32_t y = 1, uint32_t z = 1);
    void dispatchThreads(uint32_t x, uint32_t y = 1, uint32_t z = 1);
    void dispatchIndirect(BufferHandle buffer, size_t offset = 0);
    void draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0, uint32_t firstInstance = 0);
    void drawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0, int32_t vertexOffset = 0, uint32_t firstInstance = 0);
    void drawIndirect(BufferHandle buffer, size_t offset, uint32_t drawCount, uint32_t stride);
    void drawIndexedIndirect(BufferHandle buffer, size_t offset, uint32_t drawCount, uint32_t stride);

    void fillBuffer(BufferHandle buffer, uint32_t value, size_t offset = 0, size_t size = VK_WHOLE_SIZE);
    void clearImage(ImageRange image, VkClearColorValue color);
    void clearImage(ImageRange image, VkClearDepthStencilValue value);
    void copyBuffer(
        BufferHandle source, BufferHandle destination, size_t size, size_t sourceOffset = 0,
        size_t destinationOffset = 0
    );
    void copyImage(ImageRange source, ImageRange destination);

    void pushConstants(const void* data, uint32_t size);
    template <typename T>
        requires std::is_trivially_copyable_v<T>
    void pushConstants(const T& value) {
        pushConstants(&value, sizeof(T));
    }

    VkImage getImage(ImageHandle handle) const;
    VkImageView getImageView(ImageRange image, std::optional<VkImageViewType> type = std::nullopt) const;
    VkBuffer getBuffer(BufferHandle handle) const;

private:
    PassContext(RenderGraph& graph, const CommandBuffer& commandBuffer, const PassInfo& pass, VkExtent2D extent)
        : graph(graph), commandBuffer(commandBuffer), pass(pass), extent(extent) {}

    ResourceRegistry& resources() const;
    void requirePipeline(PassType type, const char* command) const;
    void requireOutsideRendering(const char* command) const;
    void requireBufferAccess(BufferHandle buffer, BufferUsageType usage, const char* command) const;
    void requireImageAccess(ImageRange image, ImageUsageType usage, const char* command) const;

    RenderGraph& graph;
    const CommandBuffer& commandBuffer;
    const PassInfo& pass;
    VkExtent2D extent;

    friend class RenderGraph;
};

}
