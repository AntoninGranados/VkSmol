#include "present_pass_builder.hpp"

#include "../builder_resource.hpp"
#include "../render_graph_builder.hpp"

namespace VkSmol {

void PresentPassBuilder::setPresentationImage(ImageHandle imageHandle) {
    graph->passes[handle.id].imageAccesses.push_back({
        .image = imageHandle,
        .info = {
            .usage = ImageUsageType::Present,
            .access = AccessType::Read,
        },
    });
}

}
