#pragma once

#include <functional>
#include <string>
#include <utility>

#include "../builder_resource.hpp"
#include "../render_graph_builder.hpp"

namespace VkSmol {

template <class Derived>
class PassBuilder {
public:
    PassHandle getHandle() const { return handle; }

    Derived& read(std::string name, ImageHandle image)   { return bind(std::move(name), true, image.id, AccessType::Read); }
    Derived& read(std::string name, BufferHandle buffer) { return bind(std::move(name), false, buffer.id, AccessType::Read); }
    Derived& write(std::string name, ImageHandle image)   { return bind(std::move(name), true, image.id, AccessType::Write); }
    Derived& write(std::string name, BufferHandle buffer) { return bind(std::move(name), false, buffer.id, AccessType::Write); }

    Derived& execute(std::function<void(PassContext&)> callback) {
        graph->passes[handle.id].execute = std::move(callback);
        return static_cast<Derived&>(*this);
    }

protected:
    PassHandle handle;
    RenderGraphBuilder* graph = nullptr;

    PassBuilder(PassHandle handle, RenderGraphBuilder* graph): handle(handle), graph(graph) {}

private:
    Derived& bind(std::string name, bool isImage, uint32_t handleId, AccessType access) {
        graph->passes[handle.id].namedBindings.push_back({
            .name     = std::move(name),
            .isImage  = isImage,
            .handleId = handleId,
            .access   = access,
        });
        return static_cast<Derived&>(*this);
    }
};

}
