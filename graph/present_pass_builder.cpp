#include "present_pass_builder.hpp"

#include "builder_resource.hpp"
#include "render_graph_builder.hpp"

void PresentPassBuilder::setPresentationImage(ImageHandle imageHandle) {
    graph->passes[handle.id].imageUsages.push_back({
        .handle = imageHandle,
        .usage = ImageUsage::Present,
        .access = AccessType::Read,
    });
}
