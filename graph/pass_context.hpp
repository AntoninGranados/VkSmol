#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <type_traits>

#include "builder_resource.hpp"

namespace VkSmol {

class RenderGraph;
class CommandBuffer;
struct CompiledPass;

class PassContext {
public:
    const CommandBuffer& getCommandBuffer() const { return commandBuffer; }
    VkCommandBuffer cmd() const;
    VkExtent2D getExtent() const { return extent; }

    void dispatch(uint32_t x, uint32_t y = 1, uint32_t z = 1);
    void draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0, uint32_t firstInstance = 0);
    void drawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0, int32_t vertexOffset = 0, uint32_t firstInstance = 0);

    void pushConstants(const void* data, uint32_t size);
    template <typename T>
        requires std::is_trivially_copyable_v<T>
    void pushConstants(const T& value) {
        pushConstants(&value, sizeof(T));
    }

    VkImage getImage(ImageHandle handle) const;
    VkImageView getImageView(ImageHandle handle) const;
    VkBuffer getBuffer(BufferHandle handle) const;

private:
    PassContext(RenderGraph& graph, const CommandBuffer& commandBuffer, const CompiledPass& pass, VkExtent2D extent)
        : graph(graph), commandBuffer(commandBuffer), pass(pass), extent(extent) {}

    RenderGraph& graph;
    const CommandBuffer& commandBuffer;
    const CompiledPass& pass;
    VkExtent2D extent;

    friend class RenderGraph;
};

}
