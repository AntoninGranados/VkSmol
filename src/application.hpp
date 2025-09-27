#pragma once

#include <string>

#include "./engine/engine.hpp"

class Application {
public:
    Application(std::string name);
    ~Application();

    void run();

private:
    Engine engine;
};
