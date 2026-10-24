#pragma once

#include <functional>
#include <string>
#include <utility>

#include "../builder_resource.hpp"

namespace VkSmol {

template <class Derived>
class PassBuilder {
public:
    PassHandle getHandle() const { return handle; }

    Derived& read(std::string name, ImageRange image) {
        return bind(std::move(name), true, image.image.id, image.subresources, AccessType::Read);
    }
    Derived& read(std::string name, BufferHandle buffer) {
        return bind(std::move(name), false, buffer.id, {}, AccessType::Read);
    }
    Derived& write(std::string name, ImageRange image) {
        return bind(std::move(name), true, image.image.id, image.subresources, AccessType::Write);
    }
    Derived& write(std::string name, BufferHandle buffer) {
        return bind(std::move(name), false, buffer.id, {}, AccessType::Write);
    }

    Derived& read(ImageRange image, ImageUsageType usage) { return access(image, usage, AccessType::Read); }
    Derived& read(BufferHandle buffer, BufferUsageType usage) { return access(buffer, usage, AccessType::Read); }
    Derived& write(ImageRange image, ImageUsageType usage) { return access(image, usage, AccessType::Write); }
    Derived& write(BufferHandle buffer, BufferUsageType usage) { return access(buffer, usage, AccessType::Write); }

    Derived& execute(std::function<void(PassContext&)> callback) {
        graph->passes[handle.id].execute = std::move(callback);
        return static_cast<Derived&>(*this);
    }

protected:
    PassHandle handle;
    GraphDeclaration* graph = nullptr;

    PassBuilder(PassHandle handle, GraphDeclaration* graph): handle(handle), graph(graph) {}

    Derived& access(ImageRange image, ImageUsageType usage, AccessType access) {
        graph->passes[handle.id].imageAccesses.push_back(
            {.image = image.image, .subresources = image.subresources, .info = {.usage = usage, .access = access}}
        );
        return static_cast<Derived&>(*this);
    }

    Derived& access(BufferHandle buffer, BufferUsageType usage, AccessType access) {
        graph->passes[handle.id].bufferAccesses.push_back({ .buffer = buffer, .info = { .usage = usage, .access = access } });
        return static_cast<Derived&>(*this);
    }

private:
    Derived&
    bind(std::string name, bool isImage, uint32_t handleId, ImageSubresourceRange subresources, AccessType access) {
        graph->passes[handle.id].namedBindings.push_back({
            .name = std::move(name),
            .isImage = isImage,
            .handleId = handleId,
            .subresources = subresources,
            .access = access,
        });
        return static_cast<Derived&>(*this);
    }
};

}
