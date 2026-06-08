#pragma once

#include "resource_types.hpp"

#include <string>
#include <vector>

class ResourceRegistry {
public:
    ResourceId create(ResourceType type, std::string name = {});
    void destroy(ResourceId id);

    void addDependency(ResourceId child, ResourceId parent);
    void removeDependency(ResourceId child, ResourceId parent);
    void removeAllParents(ResourceId child);
    bool hasDependents(ResourceId id) const;
    const std::vector<ResourceId>& getDependents(ResourceId id) const;

    bool isAlive(ResourceId id) const;
    std::vector<ResourceRecord> getAlive() const;

    ResourceRecord& get(ResourceId id);
    const ResourceRecord& get(ResourceId id) const;

private:
    std::vector<ResourceRecord> record;
    std::vector<uint32_t> freeSpaces;
};
