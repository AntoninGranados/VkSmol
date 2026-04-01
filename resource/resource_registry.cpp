#include "resource_registry.hpp"
#include "resource_reporting.hpp"

#include <algorithm>
#include <stdexcept>
#include <sstream>

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
        const uint32_t generation = record[index - 1].id.generation + 1;

        record[index-1] = {
            .id = { index, generation },
            .type = type,
            .name = std::move(name),
            .alive = true,
            .parents = {},
            .children = {},
        };

    } else {
        index = record.size()+1;

        record.push_back({
            .id = { index, 1 },
            .type = type,
            .name = std::move(name),
            .alive = true,
            .parents = {},
            .children = {},
        });
    }

    return record[index-1].id;
}

void ResourceRegistry::destroy(ResourceId id) {
    if (!isAlive(id)) {
        std::ostringstream message;
        message << "ResourceRegistry::destroy: can't destroy a dead or uninitialised resource, "
                << describeResourceSlot(record, id);
        throw std::runtime_error(message.str());
    }

    ResourceRecord& slot = record[id.index - 1];
    if (!slot.children.empty()) {
        std::ostringstream message;
        message << "ResourceRegistry::destroy: can't destroy " << formatResourceRecord(slot)
                << " because it still has dependents "
                << formatResourceList(record, slot.children);
        throw std::runtime_error(message.str());
    }

    removeAllParents(id);

    slot.type = ResourceType::Dead;
    slot.alive = false;
    slot.parents.clear();
    slot.children.clear();

    freeSpaces.push_back(id.index);
}

void ResourceRegistry::addDependency(ResourceId child, ResourceId parent) {
    if (child == parent) {
        std::ostringstream message;
        message << "ResourceRegistry::addDependency: can't add a resource dependency to itself "
                << describeResourceSlot(record, child);
        throw std::runtime_error(message.str());
    }

    ResourceRecord& childRecord = get(child);
    ResourceRecord& parentRecord = get(parent);

    if (contains(childRecord.parents, parent)) {
        std::ostringstream message;
        message << "ResourceRegistry::addDependency: dependency already exists between child "
                << formatResourceRecord(childRecord) << " and parent "
                << formatResourceRecord(parentRecord);
        throw std::runtime_error(message.str());
    }

    childRecord.parents.push_back(parent);
    parentRecord.children.push_back(child);
}

void ResourceRegistry::removeDependency(ResourceId child, ResourceId parent) {
    ResourceRecord& childRecord = get(child);
    ResourceRecord& parentRecord = get(parent);

    auto parentIt = std::find(childRecord.parents.begin(), childRecord.parents.end(), parent);
    if (parentIt == childRecord.parents.end()) {
        std::ostringstream message;
        message << "ResourceRegistry::removeDependency: missing parent dependency between child "
                << formatResourceRecord(childRecord) << " and parent "
                << formatResourceRecord(parentRecord);
        throw std::runtime_error(message.str());
    }

    auto childIt = std::find(parentRecord.children.begin(), parentRecord.children.end(), child);
    if (childIt == parentRecord.children.end()) {
        std::ostringstream message;
        message << "ResourceRegistry::removeDependency: missing child dependency between child "
                << formatResourceRecord(childRecord) << " and parent "
                << formatResourceRecord(parentRecord);
        throw std::runtime_error(message.str());
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
        std::ostringstream message;
        message << "ResourceRegistry::get: can't retrieve a dead resource, "
                << describeResourceSlot(record, id);
        throw std::runtime_error(message.str());
    }

    return record[id.index-1];
}

const ResourceRecord& ResourceRegistry::get(ResourceId id) const {
    if (!isAlive(id)) {
        std::ostringstream message;
        message << "ResourceRegistry::get: can't retrieve a dead resource, "
                << describeResourceSlot(record, id);
        throw std::runtime_error(message.str());
    }

    return record[id.index-1];
}
