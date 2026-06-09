#include "compute_pass_builder.hpp"

#include "../graph_utils.hpp"
#include "../builder_resource.hpp"
#include "../render_graph_builder.hpp"

void ComputePassBuilder::readImage(ImageHandle imageHandle, ImageUsageType usage) {
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

void ComputePassBuilder::writeImage(ImageHandle imageHandle, ImageUsageType usage) {
    checkImageAccessInfoValid({
        .usage = usage,
        .access = AccessType::Write,
    });

    graph->passes[handle.id].imageAccesses.push_back({
        .image = imageHandle,
        .info = {
            .usage = usage,
            .access = AccessType::Write,
        },
    });
}

void ComputePassBuilder::readBuffer(BufferHandle buffer, BufferUsageType usage) {
    graph->passes[handle.id].bufferAccesses.push_back({
        .buffer = buffer,
        .info = { .usage = usage, .access = AccessType::Read },
    });
}

void ComputePassBuilder::writeBuffer(BufferHandle buffer, BufferUsageType usage) {
    graph->passes[handle.id].bufferAccesses.push_back({
        .buffer = buffer,
        .info = { .usage = usage, .access = AccessType::Write },
    });
}
