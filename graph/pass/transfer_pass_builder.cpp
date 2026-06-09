#include "transfer_pass_builder.hpp"

#include "../graph_utils.hpp"
#include "../builder_resource.hpp"
#include "../render_graph_builder.hpp"

void TransferPassBuilder::copyFrom(ImageHandle imageHandle) {
    checkImageAccessInfoValid({
        .usage = ImageUsageType::TransferSrc,
        .access = AccessType::Read,
    });

    graph->passes[handle.id].imageAccesses.push_back({
        .image = imageHandle,
        .info = {
            .usage = ImageUsageType::TransferSrc,
            .access = AccessType::Read,
        },
    });
}

void TransferPassBuilder::copyTo(ImageHandle imageHandle) {
    checkImageAccessInfoValid({
        .usage = ImageUsageType::TransferDst,
        .access = AccessType::Write,
    });

    graph->passes[handle.id].imageAccesses.push_back({
        .image = imageHandle,
        .info = {
            .usage = ImageUsageType::TransferDst,
            .access = AccessType::Write,
        },
    });
}
