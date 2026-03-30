#include "resource_registry.hpp"

#include <stdexcept>
#include <algorithm>

namespace {
bool contains(const std::vector<ResourceId>& ids, ResourceId id) {
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}
}

ResourceId ResourceRegistry::create(ResourceType type, std::string name) {
    uint32_t index;

    if (!freeSpaces.empty()) {
        index = freeSpaces.back();
        freeSpaces.pop_back();

        record[index-1] = {
            .id = { index, record[index-1].id.generation + 1 },
            .type = type,
            .name = name,
            .alive = true,
            .parents = {},
            .children = {},
        };

    } else {
        index = record.size()+1;

        record.push_back({
            .id = { index, 1 },
            .type = type,
            .name = name,
            .alive = true,
            .parents = {},
            .children = {},
        });
    }

    return record[index-1].id;
}

void ResourceRegistry::destroy(ResourceId id) {
    if (!isAlive(id)) {
        throw std::runtime_error("Can't destroy a dead or uninitialised resource");
    }
    if (hasDependents(id)) {
        throw std::runtime_error("Can't destroy a resource that still has dependencies");
    }
    
    removeAllParents(id);

    ResourceRecord& record = get(id);
    record.type = ResourceType::Dead;
    record.name = {};
    record.alive = false;
    record.parents.clear();
    record.children.clear();

    freeSpaces.push_back(id.index);
}

void ResourceRegistry::addDependency(ResourceId child, ResourceId parent) {
    if (child == parent) {
        throw std::runtime_error("Can't add a resource dependency to itself");
    }

    ResourceRecord& childRecord = get(child);
    ResourceRecord& parentRecord = get(parent);

    if (contains(childRecord.parents, parent)) {
        throw std::runtime_error("Dependency already exists in resource registry");
    }

    childRecord.parents.push_back(parent);
    parentRecord.children.push_back(child);
}

void ResourceRegistry::removeDependency(ResourceId child, ResourceId parent) {
    ResourceRecord& childRecord = get(child);
    ResourceRecord& parentRecord = get(parent);

    auto parentIt = std::find(childRecord.parents.begin(), childRecord.parents.end(), parent);
    if (parentIt == childRecord.parents.end()) {
        throw std::runtime_error("Can't remove a missing parent dependency from resource registry");
    }

    auto childIt = std::find(parentRecord.children.begin(), parentRecord.children.end(), child
        );
    if (childIt == parentRecord.children.end()) {
        throw std::runtime_error("Can't remove a missing child dependency from resource registry");
    }

    childRecord.parents.erase(parentIt);
    parentRecord.children.erase(childIt);
}

void ResourceRegistry::removeAllParents(ResourceId child) {
    std::vector<ResourceId> parents = get(child).parents;
    for (ResourceId parent : parents) {
        removeDependency(child, parent);
    }
}

bool ResourceRegistry::hasDependents(ResourceId id) const {
    return !get(id).children.empty();
}

const std::vector<ResourceId>& ResourceRegistry::getDependents(ResourceId id) const {
    return get(id).children;
}


bool ResourceRegistry::isAlive(ResourceId id) const {
    return 0 < id.index && id.index <= record.size() && record[id.index-1].alive && record[id.index-1].id.generation == id.generation;
}

ResourceRecord& ResourceRegistry::get(ResourceId id) {
    if (!isAlive(id)) {
        throw std::runtime_error("Can't retrieve a dead resource");
    }

    return record[id.index-1];
}

const ResourceRecord& ResourceRegistry::get(ResourceId id) const {
    if (!isAlive(id)) {
        throw std::runtime_error("Can't retrieve a dead resource");
    }

    return record[id.index-1];
}
