#include <iostream>

#include "./engine/application.hpp"

int main() {
    #ifdef __APPLE__
        // Apple
    #endif
    #ifdef _WIN32
        // Windows
    #endif
    #ifdef __linux__
        // Linux
    #endif

    Application app;

    try {
        app.run();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
