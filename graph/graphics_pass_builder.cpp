#include "graphics_pass_builder.hpp"

#include "graph_utils.hpp"
#include "builder_resource.hpp"
#include "render_graph_builder.hpp"

void GraphicsPassBuilder::readImage(ImageHandle imageHandle, ImageUsageType usage) {
    checkUsageValidForAccess(usage, AccessType::Read);

    graph->passes[handle.id].imageAccesses.push_back({
        .image = imageHandle,
        .info {
            .usage = usage,
            .access = AccessType::Read,
        },
    });
}

void GraphicsPassBuilder::writeImage(ImageHandle imageHandle, ImageUsageType usage) {
    checkUsageValidForAccess(usage, AccessType::Write);

    graph->passes[handle.id].imageAccesses.push_back({
        .image = imageHandle,
        .info {
            .usage = usage,
            .access = AccessType::Write,
        },
    });
}
