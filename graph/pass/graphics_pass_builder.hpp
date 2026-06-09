#pragma once

#include "../builder_resource.hpp"
#include "pass_builder.hpp"

class RenderGraphBuilder;

class GraphicsPassBuilder: public PassBuilder {
public:
    void readImage(ImageHandle imageHandle, ImageUsageType usage);
    void writeImage(
        ImageHandle imageHandle,
        ImageUsageType usage,
        WriteMode writeMode = WriteMode::Overwrite,
        AttachmentLoad attachmentLoad = AttachmentLoad::DontCare
    );
    void readBuffer(BufferHandle buffer, BufferUsageType usage);
    void writeBuffer(BufferHandle buffer, BufferUsageType usage);

private:
    GraphicsPassBuilder(PassHandle handle, RenderGraphBuilder* graph)
        : PassBuilder(handle, graph) {}

    friend class RenderGraphBuilder;
};
