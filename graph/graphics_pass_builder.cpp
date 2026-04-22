#include "graphics_pass_builder.hpp"

#include "graph_utils.hpp"
#include "builder_resource.hpp"
#include "render_graph_builder.hpp"

void GraphicsPassBuilder::readImage(ImageHandle imageHandle, ImageUsage usage) {
    checkUsageValidForAccess(usage, AccessType::Read);

    graph->passes[handle.id].imageUsages.push_back({
        .handle = imageHandle,
        .usage = usage,
        .access = AccessType::Read,
    });
}

void GraphicsPassBuilder::writeImage(ImageHandle imageHandle, ImageUsage usage) {
    checkUsageValidForAccess(usage, AccessType::Write);

    graph->passes[handle.id].imageUsages.push_back({
        .handle = imageHandle,
        .usage = usage,
        .access = AccessType::Write,
    });
}
