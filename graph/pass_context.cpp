#include "pass_context.hpp"

#include <format>
#include <stdexcept>

#include "../command/command_buffer.hpp"
#include "../command/transfer_commands.hpp"
#include "../engine.hpp"
#include "graph_utils.hpp"
#include "render_graph.hpp"

namespace VkSmol {

VkCommandBuffer PassContext::cmd() const { return commandBuffer.get(); }

void PassContext::dispatch(uint32_t x, uint32_t y, uint32_t z) {
    requirePipeline(PassType::Compute, "dispatch");
    graph.pipelines.dispatch(commandBuffer, pass, x, y, z);
}

void PassContext::dispatchThreads(uint32_t x, uint32_t y, uint32_t z) {
    requirePipeline(PassType::Compute, "dispatchThreads");
    const VkExtent3D groupSize = graph.pipelines.getThreadGroupSize(pass);
    dispatch(
        (x + groupSize.width - 1) / groupSize.width, (y + groupSize.height - 1) / groupSize.height,
        (z + groupSize.depth - 1) / groupSize.depth
    );
}

void PassContext::dispatchIndirect(BufferHandle buffer, size_t offset) {
    requirePipeline(PassType::Compute, "dispatchIndirect");
    requireBufferAccess(buffer, BufferUsageType::Indirect, "dispatchIndirect");
    vkCmdDispatchIndirect(commandBuffer.get(), getBuffer(buffer), offset);
}

void PassContext::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
    requirePipeline(PassType::Graphics, "draw");
    vkCmdDraw(commandBuffer.get(), vertexCount, instanceCount, firstVertex, firstInstance);
}

void PassContext::drawIndexed(
    uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance
) {
    requirePipeline(PassType::Graphics, "drawIndexed");
    vkCmdDrawIndexed(commandBuffer.get(), indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void PassContext::drawIndirect(BufferHandle buffer, size_t offset, uint32_t drawCount, uint32_t stride) {
    requirePipeline(PassType::Graphics, "drawIndirect");
    requireBufferAccess(buffer, BufferUsageType::Indirect, "drawIndirect");
    vkCmdDrawIndirect(commandBuffer.get(), getBuffer(buffer), offset, drawCount, stride);
}

void PassContext::drawIndexedIndirect(BufferHandle buffer, size_t offset, uint32_t drawCount, uint32_t stride) {
    requirePipeline(PassType::Graphics, "drawIndexedIndirect");
    requireBufferAccess(buffer, BufferUsageType::Indirect, "drawIndexedIndirect");
    vkCmdDrawIndexedIndirect(commandBuffer.get(), getBuffer(buffer), offset, drawCount, stride);
}

void PassContext::fillBuffer(BufferHandle buffer, uint32_t value, size_t offset, size_t size) {
    requireOutsideRendering("fillBuffer");
    requireBufferAccess(buffer, BufferUsageType::TransferDst, "fillBuffer");
    TransferCommands(resources(), commandBuffer).fillBuffer(buffer, value, offset, size);
}

void PassContext::clearImage(ImageRange image, VkClearColorValue color) {
    requireOutsideRendering("clearImage");
    requireImageAccess(image, ImageUsageType::TransferDst, "clearImage");
    TransferCommands(resources(), commandBuffer).clearImage(image, color);
}

void PassContext::copyBuffer(
    BufferHandle source, BufferHandle destination, size_t size, size_t sourceOffset, size_t destinationOffset
) {
    requireOutsideRendering("copyBuffer");
    requireBufferAccess(source, BufferUsageType::TransferSrc, "copyBuffer");
    requireBufferAccess(destination, BufferUsageType::TransferDst, "copyBuffer");
    TransferCommands(resources(), commandBuffer).copyBuffer(source, destination, size, sourceOffset, destinationOffset);
}

void PassContext::copyImage(ImageRange source, ImageRange destination) {
    requireOutsideRendering("copyImage");
    requireImageAccess(source, ImageUsageType::TransferSrc, "copyImage");
    requireImageAccess(destination, ImageUsageType::TransferDst, "copyImage");
    TransferCommands(resources(), commandBuffer).copyImage(source, destination);
}

void PassContext::pushConstants(const void* data, uint32_t size) {
    graph.pipelines.pushConstants(commandBuffer, pass, data, size);
}

VkImage PassContext::getImage(ImageHandle image) const { return resources().getImage(image).image; }

VkImageView PassContext::getImageView(ImageRange image, std::optional<VkImageViewType> type) const {
    return resources().getImageView(image, type);
}

VkBuffer PassContext::getBuffer(BufferHandle buffer) const { return resources().getBuffer(buffer).buffer; }

ResourceRegistry& PassContext::resources() const { return graph.engine.getResources(); }

void PassContext::requirePipeline(PassType type, const char* command) const {
    if (pass.type != type || !pass.pipelineId)
        throw std::runtime_error(
            std::format(
                "The pass [{}] calls {} without a {} pipeline", pass.name, command,
                type == PassType::Compute ? "compute" : "graphics"
            )
        );
}

void PassContext::requireOutsideRendering(const char* command) const {
    if (pass.type == PassType::Graphics)
        throw std::runtime_error(
            std::format("The graphics pass [{}] cannot call {} while rendering", pass.name, command)
        );
}

void PassContext::requireBufferAccess(BufferHandle buffer, BufferUsageType usage, const char* command) const {
    for (const BufferAccess& access : pass.bufferAccesses)
        if (access.buffer == buffer && access.info.usage == usage) return;
    throw std::runtime_error(
        std::format(
            "The pass [{}] calls {} on the buffer [{}] without declaring it as {}", pass.name, command,
            resources().getBuffer(buffer).name, bufferUsageToStr(usage)
        )
    );
}

void PassContext::requireImageAccess(ImageRange image, ImageUsageType usage, const char* command) const {
    const ImageSubresourceRange range = resources().resolve(image);
    for (const ImageAccess& access : pass.imageAccesses)
        if (access.image == image.image && access.info.usage == usage &&
            resources().resolve(ImageRange(access.image, access.subresources)).contains(range))
            return;
    throw std::runtime_error(
        std::format(
            "The pass [{}] calls {} on [{}] of the image [{}] without declaring it as {}", pass.name, command,
            range.toString(), resources().getImage(image.image).name, imageUsageToStr(usage)
        )
    );
}

} // namespace VkSmol
