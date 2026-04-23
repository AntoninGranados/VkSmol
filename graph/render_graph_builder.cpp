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
        bool hasReadableState = imageInfo.initialAccess.has_value();

        for (const TimelineImageAccess& timelineAccess : timeline.accesses) {
            if (timelineAccess.info.access == AccessType::Write) {
                hasReadableState = true;
            } else if (timelineAccess.info.access == AccessType::Read && !hasReadableState) {
                throw std::runtime_error(std::format("Image [{}] was read but not written", imageInfo.name));
            }
        }
    }
}

std::vector<PassDependency> compilePassDependencies(const std::vector<ImageInfo>& images, const std::vector<ImageTimeline>& timelines) {
    std::vector<PassDependency> dependencies;

    for (const ImageTimeline& timeline : timelines) {
        PassHandle lastWriter = { .id = InvalidHandle };
        ImageAccessInfo lastAccess = images[timeline.image.id].initialAccess.value_or(ImageAccessInfo{});

        for (const TimelineImageAccess& timelineAccess : timeline.accesses) {
            if (timelineAccess.info.access == AccessType::Write) {
                lastWriter = timelineAccess.pass;
                lastAccess = timelineAccess.info;
            } else if (timelineAccess.info.access == AccessType::Read && lastWriter.id != InvalidHandle) {
                dependencies.push_back({
                    .src = lastWriter,
                    .dst = timelineAccess.pass,
                    .imageBarrier = {
                        .image = timeline.image,
                        .srcAccess = lastAccess,
                        .dstAccess = timelineAccess.info,
                    }
                });
            }
        }
    }

    return dependencies;
}

}

ImageHandle RenderGraphBuilder::createImage(std::string name, VkFormat format, uint32_t width, uint32_t height, uint32_t depth) {
    ImageHandle handle = { .id = static_cast<uint32_t>(images.size()) };

    images.push_back({
        .name = name,
        .format = format,
        .width = width,
        .height = height,
        .depth = depth,
    });

    return handle;
}

ImageHandle RenderGraphBuilder::importImage(std::string name, VkFormat format, uint32_t width, uint32_t height, uint32_t depth, ImageAccessInfo initialAccess, std::optional<ImageAccessInfo> finalAccess) {
    ImageHandle handle = { .id = static_cast<uint32_t>(images.size()) };

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
    compiled.passDependencies = compilePassDependencies(images, timelines);
    return compiled;
}
