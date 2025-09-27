#include "application.hpp"

Application::Application(std::string name) {
    engine.init("Application");
}

Application::~Application() {
    engine.terminate();
}

void Application::run() {
    while(!engine.shouldClose()) {
        engine.drawFrame();
    }
}
