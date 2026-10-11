#pragma once

#include <fstream>
#include <iterator>
#include <optional>
#include <string>

class ShaderFileSystem {
public:
    virtual ~ShaderFileSystem() = default;

    virtual std::optional<std::string> readFile(const std::string& path) = 0;
};

class NativeShaderFileSystem final: public ShaderFileSystem {
public:
    std::optional<std::string> readFile(const std::string& path) override {
        std::ifstream file(path, std::ios::binary);
        if (!file) return std::nullopt;
        return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }
};
