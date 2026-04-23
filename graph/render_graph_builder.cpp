#include "render_graph_builder.hpp"

#include <stdexcept>
#include <format>

#include "builder_resource.hpp"
#include "compiled_resource.hpp"
#include "graph_utils.hpp"
#include "graphics_pass_builder.hpp"
#include "transfer_pass_builder.hpp"
#include "present_pass_builder.hpp"

namespace {

struct TimelineImageAccess {
    PassHandle pass;
    ImageAccessInfo info;
};

struct ImageTimeline {
    ImageHandle image;
    std::vector<TimelineImageAccess> accesses;
};

std::vector<ImageTimeline> collectImageTimelines(const std::vector<ImageInfo>& images, const std::vector<PassInfo>& passes) {
    std::vector<ImageTimeline> timelines(images.size());

    for (uint32_t imageId = 0; imageId < images.size(); imageId++) {
        timelines[imageId].image.id = imageId;
    }

    for (uint32_t passId = 0; passId < passes.size(); passId++) {
        const PassInfo& passInfo = passes[passId];

        for (const ImageAccess& imageAccess : passInfo.imageAccesses) {
            if (imageAccess.image.id >= timelines.size()) {
                throw std::runtime_error(std::format(
                    "Pass [{}] references invalid image handle [{}]",
                    passInfo.name,
                    imageAccess.image.id
                ));
            }

            timelines[imageAccess.image.id].accesses.push_back({
                .pass = { .id = passId },
                .info = imageAccess.info,
            });
        }
    }

    return timelines;
}

void validateImageTimelines(const std::vector<ImageInfo>& images, const std::vector<ImageTimeline>& timelines) {
    for (const ImageTimeline& timeline : timelines) {
        const ImageInfo& imageInfo = images[timeline.image.id];
        bool hasReadableState =
            imageInfo.initialAccess.has_value() &&
            imageInfo.initialAccess->access == AccessType::Read &&
            imageInfo.initialAccess->usage != ImageUsageType::Undefined;

        for (const TimelineImageAccess& timelineAccess : timeline.accesses) {
            if (timelineAccess.info.access == AccessType::Write) {
                hasReadableState = true;
            } else if (timelineAccess.info.access == AccessType::Read && !hasReadableState) {
                throw std::runtime_error(std::format("Image [{}] was read but not written", imageInfo.name));
            }
        }
    }
}

bool needsBarrier(ImageAccessInfo src, ImageAccessInfo dst) {
    // @todo check src.subresourceRangeInfo == dst.subresourceRangeInfo
    return src.access != dst.access || src.usage != dst.usage;
}

std::vector<CompiledPass> computeCompiledPasses(
    const std::vector<ImageInfo>& images,
    const std::vector<PassInfo>& passes,
    const std::vector<ImageTimeline>& timelines
) {
    std::vector<CompiledPass> compiledPasses(passes.size());

    for (uint32_t passId = 0; passId < passes.size(); passId++) {
        compiledPasses[passId].pass = { .id = passId };
    }

    for (const ImageTimeline& timeline : timelines) {
        ImageAccessInfo lastAccess = images[timeline.image.id].initialAccess.value_or(ImageAccessInfo{});
        bool hasValue = images[timeline.image.id].initialAccess.has_value();

        for (const TimelineImageAccess& timelineAccess : timeline.accesses) {
            if (hasValue && needsBarrier(lastAccess, timelineAccess.info)) {
                compiledPasses[timelineAccess.pass.id].barriersBefore.push_back({
                    .image = timeline.image,
                    .srcAccess = lastAccess,
                    .dstAccess = timelineAccess.info,
                });
            }
            lastAccess = timelineAccess.info;
            hasValue = true;
        }
    }

    return compiledPasses;
}

std::vector<PassImageBarrier> computeOutputBarriers(
    const std::vector<ImageInfo>& images,
    const std::vector<ImageTimeline>& timelines
) {
    std::vector<PassImageBarrier> outputBarriers;

    for (uint32_t imageId = 0; imageId < images.size(); imageId++) {
        const ImageInfo& image = images[imageId];

        if (image.finalAccess.has_value() && !timelines[imageId].accesses.empty()) {
            if(needsBarrier(timelines[imageId].accesses.back().info, image.finalAccess.value())) {
                outputBarriers.push_back({
                    .image = { .id = imageId },
                    .srcAccess = timelines[imageId].accesses.back().info,
                    .dstAccess = image.finalAccess.value(),
                });
            }
        }
    }

    return outputBarriers;
}

} // namespace

ImageHandle RenderGraphBuilder::createImage(std::string name, VkFormat format, uint32_t width, uint32_t height, uint32_t depth) {
    ImageHandle handle = { .id = static_cast<uint32_t>(images.size()) };

    images.push_back({
        .name = name,
        .format = format,
        .width = width,
        .height = height,
        .depth = depth,

        .ownership = ImageOwnership::Transient,
    });

    return handle;
}

ImageHandle RenderGraphBuilder::importImage(std::string name, VkFormat format, uint32_t width, uint32_t height, uint32_t depth, ImageAccessInfo initialAccess, ImageAccessInfo finalAccess) {
    ImageHandle handle = { .id = static_cast<uint32_t>(images.size()) };
    checkUsageValidForAccess(initialAccess.usage, initialAccess.access);
    checkUsageValidForAccess(finalAccess.usage, finalAccess.access);

    images.push_back({
        .name = name,
        .format = format,
        .width = width,
        .height = height,
        .depth = depth,

        .ownership = ImageOwnership::Imported,
        .initialAccess = initialAccess,
        .finalAccess = finalAccess,
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
    const std::vector<ImageTimeline> timelines = collectImageTimelines(images, passes);
    validateImageTimelines(images, timelines);

    CompiledRenderGraph compiled;
    compiled.compiledPasses = computeCompiledPasses(images, passes, timelines);
    compiled.outputBarriers = computeOutputBarriers(images, timelines);
    return compiled;
}
