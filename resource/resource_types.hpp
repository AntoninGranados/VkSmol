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
    DescriptorSet,
};

struct ResourceRecord {
    ResourceId id = InvalidResourceId;
    ResourceType type = ResourceType::Dead;
    std::string name;
    bool alive = false;

    std::vector<ResourceId> parents;
    std::vector<ResourceId> children;
};
