#pragma once
#include <string>
#include <utility>

namespace VkSmol {

// Lightweight base for named Vulkan resource wrappers.
class TrackedResource {
public:
    TrackedResource() = default;
    TrackedResource(const TrackedResource&) = delete;
    TrackedResource& operator=(const TrackedResource&) = delete;
    TrackedResource(TrackedResource&& other) noexcept : name(std::move(other.name)) {}
    TrackedResource& operator=(TrackedResource&& other) noexcept {
        if (this != &other) name = std::move(other.name);
        return *this;
    }

    const std::string& getResourceName() const { return name; }
    void setResourceName(std::string n) { name = std::move(n); }

private:
    std::string name;
};

}
