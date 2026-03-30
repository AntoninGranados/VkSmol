#pragma once

#include "resource_types.hpp"

#include <string>
#include <vector>

class ResourceRegistry;

const char* resourceTypeName(ResourceType type);
std::string formatResourceId(ResourceId id);
std::string formatResourceRecord(const ResourceRecord& record);
std::string describeResourceSlot(const std::vector<ResourceRecord>& records, ResourceId id);
std::string formatResourceList(const std::vector<ResourceRecord>& records, const std::vector<ResourceId>& ids);
std::string describeTrackedResource(const ResourceRegistry& registry, ResourceId id, const char* fallbackName);
