#pragma once

#include "engine/resource/resource_types.hpp"

#include <string>
#include <utility>

template <ResourceType Type>
class TrackedResource {
public:
    TrackedResource() = default;
    TrackedResource(const TrackedResource&) = delete;
    TrackedResource& operator=(const TrackedResource&) = delete;
    TrackedResource(TrackedResource&& other) noexcept
        : id(other.id), name(std::move(other.name)) {
        other.id = InvalidResourceId;
        other.name = {};
    }
    TrackedResource& operator=(TrackedResource&& other) noexcept {
        if (this == &other) return *this;

        id = other.id;
        name = std::move(other.name);
        other.id = InvalidResourceId;
        other.name = {};
        return *this;
    }

    ResourceId getResourceId() const { return id; }
    bool hasResourceId() const { return id != InvalidResourceId; }
    const std::string& getResourceName() const { return name; }

protected:
    void setResourceId(ResourceId resourceId) { id = resourceId; }
    void resetResourceId() { id = InvalidResourceId; }
    void setResourceName(std::string resourceName) { name = std::move(resourceName); }

    friend class ResourceManager;

private:
    ResourceId id = InvalidResourceId;
    std::string name;
};
