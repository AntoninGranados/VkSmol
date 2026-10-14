#pragma once

#include <stdexcept>
#include <utility>

namespace VkSmol {

template<typename HandleT, HandleT NullHandle>
class UniqueHandle {
public:
    UniqueHandle() = default;
    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;

    UniqueHandle(UniqueHandle&& other) noexcept
        : handle(std::exchange(other.handle, NullHandle)) {}

    UniqueHandle& operator=(UniqueHandle&& other) {
        if (this == &other) return *this;
        if (handle != NullHandle) {
            throw std::runtime_error("Move assignment requires an empty destination");
        }
        handle = std::exchange(other.handle, NullHandle);
        return *this;
    }

    HandleT& get() { return handle; }
    const HandleT& get() const { return handle; }

    bool isValid() const { return handle != NullHandle; }

protected:
    void resetHandle() { handle = NullHandle; }
    HandleT& rawHandle() { return handle; }
    const HandleT& rawHandle() const { return handle; }

private:
    HandleT handle = NullHandle;
};

}
