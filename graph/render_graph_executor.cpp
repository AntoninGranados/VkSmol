#include "render_graph_executor.hpp"

#include <stdexcept>
#include <utility>

#include "../image/image_barrier.hpp"
#include "../engine.hpp"

#include "graph_utils.hpp"
#include "builder_resource.hpp"
#include "compiled_resource.hpp"
#include "render_graph_builder.hpp"

void RenderGraphExecutor::setCompiledGraph(CompiledRenderGraph _graph) {
    graph = _graph;
}
    
void RenderGraphExecutor::init(VkSmol& engine) {
    for (uint32_t id = 0; id < graph.images.size(); ++id) {
        const ImageInfo& info = graph.images[id];
        if (info.ownership == ImageOwnership::Imported) continue;

        VkImageUsageFlags usageFlags = info.extraUsageFlags;
        for (const CompiledPass& pass : graph.compiledPasses)
            for (const ImageAccess& access : pass.imageAccesses)
                if (access.image.id == id)
                    usageFlags |= imageUsageTypeToVkFlags(access.info.usage);

        Image img = engine.createImage(
            info.width, info.height, info.format,
            usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, info.name
        );
        ImageView view = engine.createImageView(img, info.name + "View");
        Sampler   smp  = engine.createSampler(info.name + "Sampler");

        ImageHandle handle = { .id = id };
        bindImage(handle, img.get(), view.get());
        ownedImages.emplace(id, std::move(img));
        ownedViews.emplace(id, std::move(view));
        ownedSamplers.emplace(id, std::move(smp));
    }
}

void RenderGraphExecutor::destroy(VkSmol& engine) {
    for (auto& [id, img] : ownedImages) {
        engine.destroySampler(ownedSamplers.at(id));
        engine.destroyImageView(ownedViews.at(id));
        engine.destroyImage(img);
        bindings.erase(id);
    }
    ownedSamplers.clear();
    ownedViews.clear();
    ownedImages.clear();
}

void RenderGraphExecutor::resize(VkSmol& engine, uint32_t width, uint32_t height) {
    destroy(engine);
    for (ImageInfo& info : graph.images) {
        if (info.ownership != ImageOwnership::Imported) {
            info.width  = width;
            info.height = height;
        }
    }
    init(engine);
}

void RenderGraphExecutor::swapBindings(const ImageHandle& a, const ImageHandle& b) {
    std::swap(bindings[a.id],     bindings[b.id]);
    std::swap(ownedImages[a.id],  ownedImages[b.id]);
    std::swap(ownedViews[a.id],   ownedViews[b.id]);
    std::swap(ownedSamplers[a.id],ownedSamplers[b.id]);
}

Image& RenderGraphExecutor::getImage(const ImageHandle& handle) {
    return ownedImages.at(handle.id);
}

const ImageView& RenderGraphExecutor::getView(const ImageHandle& handle) const {
    return ownedViews.at(handle.id);
}

const Sampler& RenderGraphExecutor::getSampler(const ImageHandle& handle) const {
    return ownedSamplers.at(handle.id);
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

const ImageBinding& RenderGraphExecutor::getBinding(const ImageHandle& handle) const {
    return bindings.at(handle.id);
}

ImageBinding RenderGraphExecutor::getTransferSrc(const PassHandle& passHandle) const {
    for (const ImageAccess& access : graph.compiledPasses[passHandle.id].imageAccesses) {
        if (access.info.usage == ImageUsageType::TransferSrc) {
            return bindings.at(access.image.id);
        }
    }
    throw std::runtime_error("No transfer src image found in pass");
}

ImageBinding RenderGraphExecutor::getTransferDst(const PassHandle& passHandle) const {
    for (const ImageAccess& access : graph.compiledPasses[passHandle.id].imageAccesses) {
        if (access.info.usage == ImageUsageType::TransferDst) {
            return bindings.at(access.image.id);
        }
    }
    throw std::runtime_error("No transfer dst image found in pass");
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
