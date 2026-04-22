#include "render_graph_builder.hpp"

#include <stdexcept>
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
    compiled = buildDependencies(compiled);
    return compiled;
}

CompiledRenderGraph RenderGraphBuilder::buildImageChains() {
    CompiledRenderGraph compiled;
    compiled.imageChains.clear();
    compiled.imageChains.resize(images.size());

    // Build chains
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

    // Validate chains
    // TODO: could be done while building but for now it is cleaner that way
    for (ImageChain& chain : compiled.imageChains) {
        for (const ImageUsageRecord& usage : chain.usages) {
            if (usage.access == AccessType::Write) {
                if (chain.lastWriter.id != InvalidHandle) {
                    throw std::runtime_error(std::format("Image [{}] was written multiple times", images[chain.image.id].name));
                }
                chain.lastWriter.id = usage.pass.id;

            } else if (usage.access == AccessType::Read) {
                if (chain.lastWriter.id == InvalidHandle) {
                    throw std::runtime_error(std::format("Image [{}] was read but not written", images[chain.image.id].name));
                }
            }
        }
    }

    return compiled;
}

CompiledRenderGraph& RenderGraphBuilder::buildDependencies(CompiledRenderGraph& compiled) {
    compiled.passDependencies.clear();

    for (ImageChain& chain : compiled.imageChains) {
        for (const ImageUsageRecord& usage : chain.usages) {
            if (usage.access == AccessType::Read) {
                compiled.passDependencies.push_back({
                    .src = chain.lastWriter,
                    .dst = usage.pass,
                    .image = chain.image,
                });
            }
        }
    }

    return compiled;
}

