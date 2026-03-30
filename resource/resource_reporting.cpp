#include "resource_reporting.hpp"

#include "resource_registry.hpp"

#include <sstream>

const char* resourceTypeName(ResourceType type) {
    switch (type) {
        case ResourceType::Dead: return "Dead";
        case ResourceType::Buffer: return "Buffer";
        case ResourceType::Image: return "Image";
        case ResourceType::ImageView: return "ImageView";
        case ResourceType::Sampler: return "Sampler";
        case ResourceType::DescriptorSet: return "DescriptorSet";
    }
    return "Unknown";
}

std::string formatResourceId(ResourceId id) {
    std::ostringstream message;
    message << '{' << id.index << ", " << id.generation << '}';
    return message.str();
}

std::string formatResourceRecord(const ResourceRecord& record) {
    std::ostringstream message;
    message << resourceTypeName(record.type) << ' ' << formatResourceId(record.id);
    message << " [" << (record.alive ? "alive" : "dead") << ']';
    return message.str();
}

std::string describeResourceSlot(const std::vector<ResourceRecord>& records, ResourceId id) {
    std::ostringstream message;
    message << "requested " << formatResourceId(id);

    if (id == InvalidResourceId) {
        message << " (invalid resource id)";
        return message.str();
    }

    if (id.index == 0 || id.index > records.size()) {
        message << " (index out of range, registry has " << records.size() << " slots)";
        return message.str();
    }

    const ResourceRecord& slot = records[id.index - 1];
    message << ", slot " << formatResourceRecord(slot);
    if (slot.id.generation != id.generation) {
        message << ", generation mismatch";
    }

    return message.str();
}

std::string formatResourceList(const std::vector<ResourceRecord>& records, const std::vector<ResourceId>& ids) {
    if (ids.empty()) return "[]";

    std::ostringstream message;
    message << '[';
    for (size_t i = 0; i < ids.size(); ++i) {
        if (i > 0) message << ", ";

        const ResourceId id = ids[i];
        if (id.index > 0 && id.index <= records.size()) {
            message << formatResourceRecord(records[id.index - 1]);
        } else {
            message << formatResourceId(id);
        }
    }
    message << ']';
    return message.str();
}

std::string describeTrackedResource(const ResourceRegistry& registry, ResourceId id, const char* fallbackName) {
    std::ostringstream message;
    if (id == InvalidResourceId) {
        message << fallbackName << ' ' << formatResourceId(id) << " [invalid]";
        return message.str();
    }

    if (!registry.isAlive(id)) {
        message << fallbackName << ' ' << formatResourceId(id) << " [dead]";
        return message.str();
    }

    const ResourceRecord& record = registry.get(id);
    message << resourceTypeName(record.type) << ' ' << formatResourceId(record.id) << " [alive]";
    return message.str();
}
