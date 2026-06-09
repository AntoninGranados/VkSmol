#include "render_graph_builder.hpp"

#include <stdexcept>
#include <format>

#include "builder_resource.hpp"
#include "compiled_resource.hpp"
#include "graph_utils.hpp"
#include "graphics_pass_builder.hpp"
#include "compute_pass_builder.hpp"
#include "transfer_pass_builder.hpp"
#include "present_pass_builder.hpp"
#include "pass_builder.hpp"

void PassBuilder::setGroup(SubmissionGroupHandle group) {
    graph->passes[handle.id].group = group;
}

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
        compiledPasses[passId].pass = { .id = passId };
        compiledPasses[passId].imageAccesses  = passes[passId].imageAccesses;
        compiledPasses[passId].bufferAccesses = passes[passId].bufferAccesses;
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

std::vector<CompiledSubmissionGroup> computeCompiledSubmissionGroups(
    const std::vector<SubmissionGroupInfo>& groups,
    const std::vector<PassInfo>& passes,
    const std::vector<ImageInfo>& images,
    const std::vector<ImageTimeline>& timelines,
    const std::vector<BufferTimeline>& bufferTimelines
) {
    uint32_t numGroups = groups.empty() ? 1 : static_cast<uint32_t>(groups.size());

    std::vector<CompiledSubmissionGroup> compiled(numGroups);
    for (uint32_t g = 0; g < numGroups; ++g) {
        compiled[g].handle = { .id = g };
        compiled[g].name   = groups.empty() ? "DefaultGroup" : groups[g].name;
    }

    // Assign passes to groups; unassigned passes default to group 0
    for (uint32_t p = 0; p < passes.size(); ++p) {
        uint32_t gid = passes[p].group.id;
        if (gid == InvalidHandle || gid >= numGroups) gid = 0;
        compiled[gid].passes.push_back({ .id = p });
    }

    // Derive cross-group dependency edges from image timelines
    auto addDep = [&](std::vector<SubmissionGroupHandle>& deps, uint32_t fromId) {
        SubmissionGroupHandle from = { .id = fromId };
        if (std::find_if(deps.begin(), deps.end(), [&](auto& d){ return d.id == from.id; }) == deps.end())
            deps.push_back(from);
    };

    for (const ImageTimeline& timeline : timelines) {
        uint32_t lastGroup = InvalidHandle;
        for (const TimelineImageAccess& access : timeline.accesses) {
            uint32_t gid = passes[access.pass.id].group.id;
            if (gid == InvalidHandle || gid >= numGroups) gid = 0;
            if (lastGroup != InvalidHandle && lastGroup != gid)
                addDep(compiled[gid].dependencies, lastGroup);
            lastGroup = gid;
        }
    }

    // Derive cross-group dependency edges from buffer timelines
    for (const BufferTimeline& timeline : bufferTimelines) {
        uint32_t lastGroup = InvalidHandle;
        for (const TimelineBufferAccess& access : timeline.accesses) {
            uint32_t gid = passes[access.pass.id].group.id;
            if (gid == InvalidHandle || gid >= numGroups) gid = 0;
            if (lastGroup != InvalidHandle && lastGroup != gid)
                addDep(compiled[gid].dependencies, lastGroup);
            lastGroup = gid;
        }
    }

    // Mark which group waits on imageAvailable (first write to the swapchain / Present image)
    for (const ImageTimeline& timeline : timelines) {
        const ImageInfo& info = images[timeline.image.id];
        if (info.ownership != ImageOwnership::Imported) continue;
        if (!info.finalAccess.has_value() || info.finalAccess->usage != ImageUsageType::Present) continue;
        for (const TimelineImageAccess& access : timeline.accesses) {
            if (access.info.access == AccessType::Write) {
                uint32_t gid = passes[access.pass.id].group.id;
                if (gid == InvalidHandle || gid >= numGroups) gid = 0;
                compiled[gid].waitsOnImageAvailable = true;
                compiled[gid].imageAvailableWaitStage = imageAccessToVk(access.info).stage;
                break;
            }
        }
        break;
    }

    // Mark which group signals the present semaphore (group containing the PresentPass)
    for (uint32_t p = 0; p < passes.size(); ++p) {
        if (passes[p].type == PassType::Present) {
            uint32_t gid = passes[p].group.id;
            if (gid == InvalidHandle || gid >= numGroups) gid = 0;
            compiled[gid].signalsPresentSemaphore = true;
            break;
        }
    }

    return compiled;
}

} // namespace

SubmissionGroupHandle RenderGraphBuilder::addSubmissionGroup(std::string name) {
    SubmissionGroupHandle handle = { .id = static_cast<uint32_t>(groups.size()) };
    groups.push_back({ .name = std::move(name) });
    return handle;
}

ImageHandle RenderGraphBuilder::createImage(std::string name, VkFormat format, uint32_t width, uint32_t height, uint32_t depth, VkImageUsageFlags extraUsageFlags) {
    ImageHandle handle = { .id = static_cast<uint32_t>(images.size()) };

    images.push_back({
        .name = name,
        .format = format,
        .width = width,
        .height = height,
        .depth = depth,
        .ownership = ImageOwnership::Transient,
        .extraUsageFlags = extraUsageFlags,
    });

    return handle;
}

ImageHandle RenderGraphBuilder::createImage(std::string name, VkFormat format, uint32_t width, uint32_t height, uint32_t depth, ImageAccessInfo initialAccess, ImageAccessInfo finalAccess, VkImageUsageFlags extraUsageFlags) {
    ImageHandle handle = { .id = static_cast<uint32_t>(images.size()) };
    checkImageAccessInfoValid(initialAccess);
    checkImageAccessInfoValid(finalAccess);

    images.push_back({
        .name = name,
        .format = format,
        .width = width,
        .height = height,
        .depth = depth,
        .ownership = ImageOwnership::Managed,
        .initialAccess = initialAccess,
        .finalAccess = finalAccess,
        .extraUsageFlags = extraUsageFlags,
    });

    return handle;
}

ImageHandle RenderGraphBuilder::importImage(std::string name, VkFormat format, uint32_t width, uint32_t height, uint32_t depth, ImageAccessInfo initialAccess, ImageAccessInfo finalAccess) {
    ImageHandle handle = { .id = static_cast<uint32_t>(images.size()) };
    checkImageAccessInfoValid(initialAccess);
    checkImageAccessInfoValid(finalAccess);

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

BufferHandle RenderGraphBuilder::createBuffer(std::string name, size_t size, VkBufferUsageFlags extraUsageFlags) {
    BufferHandle handle = { .id = static_cast<uint32_t>(buffers.size()) };
    buffers.push_back({
        .name = std::move(name),
        .size = size,
        .ownership = BufferOwnership::Managed,
        .extraUsageFlags = extraUsageFlags,
    });
    return handle;
}

BufferHandle RenderGraphBuilder::createStaticBuffer(std::string name, const void* data, size_t size, VkBufferUsageFlags extraUsageFlags) {
    BufferHandle handle = { .id = static_cast<uint32_t>(buffers.size()) };
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    buffers.push_back({
        .name = std::move(name),
        .size = size,
        .ownership = BufferOwnership::Managed,
        .extraUsageFlags = extraUsageFlags,
        .initialData = std::vector<uint8_t>(bytes, bytes + size),
    });
    return handle;
}

BufferHandle RenderGraphBuilder::createPerFrameBuffer(std::string name, size_t size, VkBufferUsageFlags extraUsageFlags) {
    BufferHandle handle = { .id = static_cast<uint32_t>(buffers.size()) };
    buffers.push_back({
        .name = std::move(name),
        .size = size,
        .ownership = BufferOwnership::Managed,
        .extraUsageFlags = extraUsageFlags,
        .perFrame = true,
    });
    return handle;
}

BufferHandle RenderGraphBuilder::importBuffer(std::string name) {
    BufferHandle handle = { .id = static_cast<uint32_t>(buffers.size()) };
    buffers.push_back({
        .name = std::move(name),
        .ownership = BufferOwnership::Imported,
    });
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

PresentPassBuilder RenderGraphBuilder:: addPresentPass(std::string name) {
    return addPass<PresentPassBuilder, PassType::Present>(name);
}

CompiledRenderGraph RenderGraphBuilder::compile() {
    const std::vector<ImageTimeline>  imageTimelines  = collectImageTimelines(images, passes);
    const std::vector<BufferTimeline> bufferTimelines = collectBufferTimelines(buffers, passes);
    validateImageTimelines(images, imageTimelines);

    CompiledRenderGraph compiled;
    compiled.compiledPasses   = computeCompiledPasses(images, passes, imageTimelines, bufferTimelines);
    compiled.outputBarriers   = computeOutputBarriers(images, imageTimelines);
    compiled.images           = images;
    compiled.buffers          = buffers;
    compiled.submissionGroups = computeCompiledSubmissionGroups(groups, passes, images, imageTimelines, bufferTimelines);
    return compiled;
}
