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

     VkApplicationInfo appInfo {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = windowName,
        .apiVersion = VK_API_VERSION_1_3
    };

    const char* missingLayer = "VK_LAYER_DOES_NOT_EXIST_FOR_TESTING";

    VkInstanceCreateInfo instanceCI {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = 1,
        .ppEnabledLayerNames = &missingLayer
    };

    checkError<&vkCreateInstance>(
        vkCreateInstance(&instanceCI, nullptr, &instance_)
    );

    // checkError<&vkCreateInstance>(instanceCI, nullptr, &instance_); 

}

void Engine::initWindow() {

}

void Engine::loop() {

}

void Engine::cleanup() {

}

template<auto Fn>
VkResult Engine::checkError(VkResult result) {

    // get the function's name
    constexpr std::string_view funcName =
        std::meta::identifier_of(std::meta::reflect_function(*Fn));

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