#include <print>
#include <stdexcept>

#include "engine.hpp"

int main() {

    Engine app("vulkan-learn", 1080, 720);

    try {

        app.run();

    } catch (const std::exception& e) {

        std::println(stderr, "{}", e.what());
        return EXIT_FAILURE;

    } return EXIT_SUCCESS;

}