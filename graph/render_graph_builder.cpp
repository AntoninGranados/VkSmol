#include "render_graph_builder.hpp"

#include <stdexcept>
#include <format>

#include "builder_resource.hpp"
#include "compiled_resource.hpp"
#include "graph_utils.hpp"
#include "pass/graphics_pass_builder.hpp"
#include "pass/compute_pass_builder.hpp"
#include "pass/transfer_pass_builder.hpp"

namespace VkSmol {

namespace {

struct TimelineImageAccess {
    PassHandle pass;
    ImageAccessInfo info;
};

struct ImageTimeline {
    ImageHandle image;
    std::vector<TimelineImageAccess> accesses;
};

struct TimelineBufferAccess {
    PassHandle pass;
    BufferAccessInfo info;
};

struct BufferTimeline {
    BufferHandle buffer;
    std::vector<TimelineBufferAccess> accesses;
};

bool hasDefinedContents(const std::optional<ImageAccessInfo>& access) {
    return access.has_value() &&
        access->usage != ImageUsageType::Undefined &&
        access->access != AccessType::None;
}

bool requiresDefinedContents(const ImageAccessInfo& access) {
    return access.access == AccessType::Read || access.writeMode == WriteMode::Preserve;
}

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
        bool hasPreviousContents = hasDefinedContents(imageInfo.initialAccess);

        for (const TimelineImageAccess& timelineAccess : timeline.accesses) {
            // Transient images are created fresh (UNDEFINED layout); the executor always
            // emits an UNDEFINED→dst barrier, so reading uninitialized data is intentional.
            if (imageInfo.ownership == ImageOwnership::Transient) {
                if (timelineAccess.info.access == AccessType::Write)
                    hasPreviousContents = true;
                continue;
            }

            if (requiresDefinedContents(timelineAccess.info) && !hasPreviousContents) {
                throw std::runtime_error(std::format(
                    "Image [{}] requires previous contents before [{}] access",
                    imageInfo.name,
                    imageUsageToStr(timelineAccess.info.usage)
                ));
            }

            if (timelineAccess.info.access == AccessType::Write) {
                hasPreviousContents = true;
            }
        }
    }
}

bool needsBarrier(ImageAccessInfo src, ImageAccessInfo dst) {
    // @todo check src.subresourceRangeInfo == dst.subresourceRangeInfo
    return src.access != dst.access || src.usage != dst.usage;
}

std::vector<BufferTimeline> collectBufferTimelines(
    const std::vector<BufferInfo>& buffers,
    const std::vector<PassInfo>& passes
) {
    std::vector<BufferTimeline> timelines(buffers.size());
    for (uint32_t i = 0; i < buffers.size(); ++i)
        timelines[i].buffer.id = i;

    for (uint32_t passId = 0; passId < passes.size(); ++passId) {
        for (const BufferAccess& access : passes[passId].bufferAccesses) {
            timelines[access.buffer.id].accesses.push_back({
                .pass = { .id = passId },
                .info = access.info,
            });
        }
    }
    return timelines;
}

bool needsBufferBarrier(BufferAccessInfo src, BufferAccessInfo dst) {
    return !(src.access == AccessType::Read && dst.access == AccessType::Read);
}

std::vector<CompiledPass> computeCompiledPasses(
    const std::vector<ImageInfo>& images,
    const std::vector<PassInfo>& passes,
    const std::vector<ImageTimeline>& imageTimelines,
    const std::vector<BufferTimeline>& bufferTimelines
) {
    std::vector<CompiledPass> compiledPasses(passes.size());

    for (uint32_t passId = 0; passId < passes.size(); passId++) {
        compiledPasses[passId].pass               = { .id = passId };
        compiledPasses[passId].name               = passes[passId].name;
        compiledPasses[passId].type               = passes[passId].type;
        compiledPasses[passId].imageAccesses       = passes[passId].imageAccesses;
        compiledPasses[passId].bufferAccesses      = passes[passId].bufferAccesses;
        compiledPasses[passId].descriptorBindings  = passes[passId].descriptorBindings;
    }

    for (const ImageTimeline& timeline : imageTimelines) {
        ImageAccessInfo lastAccess = images[timeline.image.id].initialAccess.value_or(ImageAccessInfo{
            .usage = ImageUsageType::Undefined,
            .access = AccessType::None,
        });

        for (const TimelineImageAccess& timelineAccess : timeline.accesses) {
            if (needsBarrier(lastAccess, timelineAccess.info)) {
                compiledPasses[timelineAccess.pass.id].barriersBefore.push_back({
                    .image = timeline.image,
                    .srcAccess = lastAccess,
                    .dstAccess = timelineAccess.info,
                });
            }
            lastAccess = timelineAccess.info;
        }
    }

    for (const BufferTimeline& timeline : bufferTimelines) {
        if (timeline.accesses.empty()) continue;

        BufferAccessInfo lastAccess = timeline.accesses[0].info;
        for (uint32_t i = 1; i < timeline.accesses.size(); ++i) {
            const TimelineBufferAccess& cur = timeline.accesses[i];
            if (needsBufferBarrier(lastAccess, cur.info)) {
                compiledPasses[cur.pass.id].bufferBarriersBefore.push_back({
                    .buffer = timeline.buffer,
                    .srcAccess = lastAccess,
                    .dstAccess = cur.info,
                });
            }
            lastAccess = cur.info;
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

ImageHandle RenderGraphBuilder::createImage(
    std::string name, VkFormat format,
    uint32_t width, uint32_t height, uint32_t depth,
    ImageOwnership ownership,
    VkImageUsageFlags extraUsageFlags,
    std::optional<ImageAccessInfo> initialAccess,
    std::optional<ImageAccessInfo> finalAccess
) {
    if (ownership == ImageOwnership::Managed && (!initialAccess.has_value() || !finalAccess.has_value()))
        throw std::runtime_error("Managed images require both initialAccess and finalAccess");
    if (ownership == ImageOwnership::Transient && (initialAccess.has_value() || finalAccess.has_value()))
        throw std::runtime_error("Transient images must not specify initialAccess/finalAccess");

    if (initialAccess.has_value()) checkImageAccessInfoValid(*initialAccess);
    if (finalAccess.has_value()) checkImageAccessInfoValid(*finalAccess);

    ImageHandle handle = { .id = static_cast<uint32_t>(images.size()) };
    images.push_back({
        .name = std::move(name),
        .format = format,
        .width = width,
        .height = height,
        .depth = depth,
        .ownership = ownership,
        .initialAccess = initialAccess,
        .finalAccess = finalAccess,
        .extraUsageFlags = ownership == ImageOwnership::Imported ? 0 : extraUsageFlags,
    });

    return handle;
}

BufferHandle RenderGraphBuilder::createBuffer(
    std::string name, size_t size,
    BufferOwnership ownership,
    VkBufferUsageFlags extraUsageFlags,
    const void* initialData
) {
    if (initialData && ownership == BufferOwnership::Imported)
        throw std::runtime_error("Imported buffers cannot have initial data");

    BufferHandle handle = { .id = static_cast<uint32_t>(buffers.size()) };
    BufferInfo info = {
        .name = std::move(name),
        .size = size,
        .ownership = ownership,
        .extraUsageFlags = extraUsageFlags,
    };
    if (initialData) {
        const uint8_t* bytes = static_cast<const uint8_t*>(initialData);
        info.initialData = std::vector<uint8_t>(bytes, bytes + size);
    }
    buffers.push_back(std::move(info));

    return handle;
}

GraphicsPassBuilder RenderGraphBuilder::addGraphicsPass(std::string name) {
    return addPass<GraphicsPassBuilder, PassType::Graphics>(name);
}

ComputePassBuilder RenderGraphBuilder::addComputePass(std::string name) {
    return addPass<ComputePassBuilder, PassType::Compute>(name);
}

TransferPassBuilder RenderGraphBuilder::addTransferPass(std::string name) {
    return addPass<TransferPassBuilder, PassType::Transfer>(name);
}

PassHandle RenderGraphBuilder::addPresentPass(ImageHandle image) {
    PassHandle handle = { .id = static_cast<uint32_t>(passes.size()) };
    passes.push_back({
        .name = "Present",
        .type = PassType::Present,
        .imageAccesses = { { .image = image, .info = { .usage = ImageUsageType::Present, .access = AccessType::Read } } },
    });
    return handle;
}

CompiledRenderGraph RenderGraphBuilder::compile() {
    const std::vector<ImageTimeline>  imageTimelines  = collectImageTimelines(images, passes);
    const std::vector<BufferTimeline> bufferTimelines = collectBufferTimelines(buffers, passes);
    validateImageTimelines(images, imageTimelines);

    CompiledRenderGraph compiled;
    compiled.compiledPasses    = computeCompiledPasses(images, passes, imageTimelines, bufferTimelines);
    compiled.outputBarriers    = computeOutputBarriers(images, imageTimelines);
    compiled.images            = images;
    compiled.buffers           = buffers;
    compiled.computePipelines  = computePipelines;
    compiled.graphicsPipelines = graphicsPipelines;

    for (uint32_t i = 0; i < computePipelines.size(); ++i)
        if (computePipelines[i].passId != InvalidHandle)
            compiled.compiledPasses[computePipelines[i].passId].pipelineId = i;
    for (uint32_t i = 0; i < graphicsPipelines.size(); ++i)
        if (graphicsPipelines[i].passId != InvalidHandle)
            compiled.compiledPasses[graphicsPipelines[i].passId].pipelineId = i;

    return compiled;
}

}
