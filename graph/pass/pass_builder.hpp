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

    Derived& read(std::string name, ImageHandle image)    { return bind(std::move(name), true, image.id, AccessType::Read, image.persistent); }
    Derived& read(std::string name, BufferHandle buffer)  { return bind(std::move(name), false, buffer.id, AccessType::Read, buffer.persistent); }
    Derived& write(std::string name, ImageHandle image)   { return bind(std::move(name), true, image.id, AccessType::Write, image.persistent); }
    Derived& write(std::string name, BufferHandle buffer) { return bind(std::move(name), false, buffer.id, AccessType::Write, buffer.persistent); }

    Derived& read(ImageHandle image, ImageUsageType usage)    { return access(image, usage, AccessType::Read); }
    Derived& read(BufferHandle buffer, BufferUsageType usage) { return access(buffer, usage, AccessType::Read); }
    Derived& write(ImageHandle image, ImageUsageType usage)    { return access(image, usage, AccessType::Write); }
    Derived& write(BufferHandle buffer, BufferUsageType usage) { return access(buffer, usage, AccessType::Write); }

    Derived& execute(std::function<void(PassContext&)> callback) {
        graph->passes[handle.id].execute = std::move(callback);
        return static_cast<Derived&>(*this);
    }

protected:
    PassHandle handle;
    RenderGraphBuilder* graph = nullptr;

    PassBuilder(PassHandle handle, RenderGraphBuilder* graph): handle(handle), graph(graph) {}

    Derived& access(ImageHandle image, ImageUsageType usage, AccessType access) {
        graph->passes[handle.id].imageAccesses.push_back({ .image = image, .info = { .usage = usage, .access = access } });
        return static_cast<Derived&>(*this);
    }

    Derived& access(BufferHandle buffer, BufferUsageType usage, AccessType access) {
        graph->passes[handle.id].bufferAccesses.push_back({ .buffer = buffer, .info = { .usage = usage, .access = access } });
        return static_cast<Derived&>(*this);
    }

private:
    Derived& bind(std::string name, bool isImage, uint32_t handleId, AccessType access, bool persistent) {
        graph->passes[handle.id].namedBindings.push_back({
            .name       = std::move(name),
            .isImage    = isImage,
            .handleId   = handleId,
            .access     = access,
            .persistent = persistent,
        });
        return static_cast<Derived&>(*this);
    }
};

}
