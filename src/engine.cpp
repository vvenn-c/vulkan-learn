#include <print>
#include <string>
#include <stdexcept>

#include <vulkan/vulkan.h>

#include "engine.hpp"

void Engine::run() {

    initVulkan();
    initWindow();
    loop();
    cleanup();

}

void Engine::initVulkan() {

    

}

void Engine::initWindow() {

}

void Engine::loop() {

}

void Engine::cleanup() {

}

void Engine::checkError(VkResult result, const std::string& funcName) {

    if (result < VK_SUCCESS) {

        throw std::runtime_error(
            std::format(
                "vulkan function {}() returned an error: {}",
                funcName,
                static_cast<int>(result)
            )
        );

    } else if (result > VK_SUCCESS) {

        std::println(
            stderr,
            "vulkan function {}() returned status: {}",
            funcName,
            static_cast<int>(result)
        );

    }

}