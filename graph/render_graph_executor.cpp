#include "render_graph_executor.hpp"

#include "../image/image_barrier.hpp"

#include "graph_utils.hpp"
#include "builder_resource.hpp"
#include "compiled_resource.hpp"
#include "render_graph_builder.hpp"

void RenderGraphExecutor::setCompiledGraph(CompiledRenderGraph _graph) {
    graph = _graph;
}
    
void RenderGraphExecutor::bindImage(const ImageHandle& handle, VkImage image, VkImageView view) {
    bindings[handle.id] = {
        .image = image,
        .view = view,
    };
}

void RenderGraphExecutor::emitBarriers(const CommandBuffer& commandBuffer, const PassHandle& passHandle) const {
    for (const PassImageBarrier& barrier : graph.compiledPasses[passHandle.id].barriersBefore) {
        emitBarrier(commandBuffer, barrier);
    }
}

void RenderGraphExecutor::emitOutputBarriers(const CommandBuffer& commandBuffer) const {
    for (const PassImageBarrier& barrier : graph.outputBarriers) {
        emitBarrier(commandBuffer, barrier);
    }
}

std::vector<AttachmentInfo> RenderGraphExecutor::getColorAttachment(const PassHandle& passHandle) const {
    std::vector<AttachmentInfo> attachments = {};

    for (const ImageAccess& access : graph.compiledPasses[passHandle.id].imageAccesses) {
        if (access.info.usage == ImageUsageType::ColorAttachment) {
            const ImageBinding& binding = bindings.at(access.image.id);
            VulkanImageState state = imageAccessToVk(access.info);
            attachments.push_back({
                .view   = binding.view,
                .layout = state.layout,
                .loadOp = attachmentLoadToVk(access.info.attachmentLoad),
            });
        }
    }
    if (attachments.size() == 0) throw std::runtime_error("No color attachment found in pass");

    return attachments;
}

void RenderGraphExecutor::emitBarrier(const CommandBuffer& commandBuffer, const PassImageBarrier& barrier) const {
    ImageBinding binding = bindings.at(barrier.image.id);
    
    VulkanImageState src = imageAccessToVk(barrier.srcAccess);
    VulkanImageState dst = imageAccessToVk(barrier.dstAccess);
    
    ImageBarrier b;
    b.call(
        commandBuffer,
        binding.image,
        src.layout, dst.layout,
        src.access, dst.access,
        src.stage, dst.stage,
        dst.aspect
    );
}
