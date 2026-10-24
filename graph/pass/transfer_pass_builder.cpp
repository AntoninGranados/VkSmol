#include "transfer_pass_builder.hpp"

#include "../graph_utils.hpp"
#include "../builder_resource.hpp"

namespace VkSmol {

TransferPassBuilder& TransferPassBuilder::copyFrom(ImageRange image) {
    checkImageAccessInfoValid({
        .usage = ImageUsageType::TransferSrc,
        .access = AccessType::Read,
    });

    graph->passes[handle.id].imageAccesses.push_back({
        .image = image.image,
        .subresources = image.subresources,
        .info = {
            .usage = ImageUsageType::TransferSrc,
            .access = AccessType::Read,
        },
    });
    return *this;
}

TransferPassBuilder& TransferPassBuilder::copyFrom(BufferHandle buffer) {
    return access(buffer, BufferUsageType::TransferSrc, AccessType::Read);
}

TransferPassBuilder& TransferPassBuilder::copyTo(BufferHandle buffer) {
    return access(buffer, BufferUsageType::TransferDst, AccessType::Write);
}

TransferPassBuilder& TransferPassBuilder::copyTo(ImageRange image) {
    checkImageAccessInfoValid({
        .usage = ImageUsageType::TransferDst,
        .access = AccessType::Write,
    });

    graph->passes[handle.id].imageAccesses.push_back({
        .image = image.image,
        .subresources = image.subresources,
        .info = {
            .usage = ImageUsageType::TransferDst,
            .access = AccessType::Write,
        },
    });
    return *this;
}
}
