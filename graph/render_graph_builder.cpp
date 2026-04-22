#include "render_graph_builder.hpp"

#include <iostream>
#include <format>

#include "builder_resource.hpp"
#include "compiled_resource.hpp"
#include "graph_utils.hpp"
#include "graphics_pass_builder.hpp"
#include "transfer_pass_builder.hpp"
#include "present_pass_builder.hpp"


ImageHandle RenderGraphBuilder::createImage(std::string name, VkFormat format, uint32_t width, uint32_t height, uint32_t depth, uint32_t arrayLayers, uint32_t mipLevels) {
    ImageHandle handle = { .id = static_cast<uint32_t>(images.size()) };

    images.push_back({
        .name = name,
        .format = format,
        .width = width,
        .height = height,
        .depth = depth,
        .arrayLayers = arrayLayers,
        .mipLevels = mipLevels,
    });

    return handle;
}

GraphicsPassBuilder RenderGraphBuilder::addGraphicsPass(std::string name) {
    return addPass<GraphicsPassBuilder, PassType::Graphics>(name);
}

TransferPassBuilder RenderGraphBuilder::addTransferPass(std::string name) {
    return addPass<TransferPassBuilder, PassType::Transfer>(name);
}

PresentPassBuilder RenderGraphBuilder:: addPresentPass(std::string name) {
    return addPass<PresentPassBuilder, PassType::Present>(name);
}

CompiledRenderGraph RenderGraphBuilder::compile() {
    CompiledRenderGraph compiled = buildImageChains();
    return compiled;
}

CompiledRenderGraph RenderGraphBuilder::buildImageChains() {
    CompiledRenderGraph compiled;
    compiled.imageChains.resize(images.size());

    for (uint32_t imageId = 0; imageId < images.size(); imageId++) {
        compiled.imageChains[imageId].image = ImageHandle{ .id = imageId };
    }

    for (uint32_t passId = 0; passId < passes.size(); passId++) {
        const PassInfo& passInfo = passes[passId];

        for (const ImageUsageInfo& usage : passInfo.imageUsages) {
            compiled.imageChains[usage.handle.id].usages.push_back({
                .pass = PassHandle{ .id = passId },
                .usage = usage.usage,
                .access = usage.access,
            });
        }
    }

    return compiled;
}

