#include "application.hpp"

Application::Application(std::string name) {
    engine.init("Application", VK_MAKE_API_VERSION(0, 1, 0, 0));
}

Application::~Application() {
    engine.terminate();
}

void Application::run() {
    while(!engine.shouldTerminate()) {
        engine.drawFrame();
    }
}
