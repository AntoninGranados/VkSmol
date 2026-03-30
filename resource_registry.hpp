#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct ResourceId {
    uint32_t index = 0;
    uint32_t generation = 0;

    bool operator==(const ResourceId&) const = default;
};
constexpr ResourceId InvalidResourceId = { 0, 0 };

enum class ResourceType {
    Dead,

    Buffer,
    Image,
    ImageView,
    Sampler,
    DescriptorSetGroup,
};

struct ResourceRecord {
    ResourceId id = InvalidResourceId;
    ResourceType type = ResourceType::Dead;
    std::string name;
    bool alive = false;

    std::vector<ResourceId> parents;
    std::vector<ResourceId> children;
};

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

    ResourceRecord& get(ResourceId id);
    const ResourceRecord& get(ResourceId id) const;

private:
    std::vector<ResourceRecord> record;
    std::vector<uint32_t> freeSpaces;
};
