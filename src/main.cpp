#include <iostream>

#include "./engine/engine.hpp"

int main()  {
    #ifdef __APPLE__
        // Apple
    #endif
    #ifdef _WIN32
        // Windows
    #endif
    #ifdef __linux__
        // Linux
    #endif

    Engine engine;
    engine.init("Application");

    while (!engine.shouldClose()) {
        engine.drawFrame();
    }

    engine.terminate();
    return EXIT_SUCCESS;
}
