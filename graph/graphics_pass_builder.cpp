#include "graphics_pass_builder.hpp"

#include "graph_utils.hpp"
#include "builder_resource.hpp"
#include "render_graph_builder.hpp"

void GraphicsPassBuilder::readImage(ImageHandle imageHandle, ImageUsageType usage) {
    checkImageAccessInfoValid({
        .usage = usage,
        .access = AccessType::Read,
    });

    graph->passes[handle.id].imageAccesses.push_back({
        .image = imageHandle,
        .info = {
            .usage = usage,
            .access = AccessType::Read,
        },
    });
}

void GraphicsPassBuilder::writeImage(
    ImageHandle imageHandle,
    ImageUsageType usage,
    WriteMode writeMode,
    AttachmentLoad attachmentLoad
) {
    checkImageAccessInfoValid({
        .usage = usage,
        .access = AccessType::Write,
        .writeMode = writeMode,
        .attachmentLoad = attachmentLoad,
    });

    graph->passes[handle.id].imageAccesses.push_back({
        .image = imageHandle,
        .info = {
            .usage = usage,
            .access = AccessType::Write,
            .writeMode = writeMode,
            .attachmentLoad = attachmentLoad,
        },
    });
}
