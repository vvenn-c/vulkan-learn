#include <print>
#include <stdexcept>
#include <cstdio>
#include <meta>
#include <string_view>

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

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

template<auto func>
VkResult Engine::checkError(const VkResult result) {

    // get the function's name
    constexpr std::string_view funcName =
        std::meta::identifier_of(std::meta::reflect_function(*func));

    if (result < VK_SUCCESS) {

        throw std::runtime_error(
            std::format(
                "vulkan function {}() returned an error: {}",
                funcName,
                string_VkResult(result)
            )
        );

    }

    if (result > VK_SUCCESS) {

        std::println(
            stderr,
            "vulkan function {}() returned status: {}",
            funcName,
            string_VkResult(result)
        );
        
    }

    return result;

}