#pragma once

#include "../resource_registry.hpp"

template <ResourceType Type>
class TrackedResource {
public:
    TrackedResource() = default;
    TrackedResource(const TrackedResource&) = delete;
    TrackedResource& operator=(const TrackedResource&) = delete;
    TrackedResource(TrackedResource&& other) noexcept
        : resourceId(other.resourceId) {
        other.resourceId = InvalidResourceId;
    }
    TrackedResource& operator=(TrackedResource&& other) noexcept {
        if (this == &other) return *this;

        resourceId = other.resourceId;
        other.resourceId = InvalidResourceId;
        return *this;
    }

    ResourceId getResourceId() const { return resourceId; }
    bool hasResourceId() const { return resourceId != InvalidResourceId; }

protected:
    void setResourceId(ResourceId id) { resourceId = id; }
    void resetResourceId() { resourceId = InvalidResourceId; }

    friend class VkSmol;

private:
    ResourceId resourceId = InvalidResourceId;
};
