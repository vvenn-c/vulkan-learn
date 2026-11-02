#include "engine.hpp"

void Engine::run() {

    initWindow();
    initVulkan();

    loop();

    cleanup();

}

void Engine::initWindow() {

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "vulkan-learn", nullptr, nullptr);

}

void Engine::initVulkan() {

    if (createInstance() != VK_SUCCESS)
        throw std::runtime_error("failed to create vulkan instance");

}

void Engine::loop() {
    while (!glfwWindowShouldClose(window)) {

        glfwPollEvents();

    }
}

void Engine::cleanup() {

    vkDestroyInstance(instance, nullptr);

    glfwDestroyWindow(window);
    glfwTerminate();

}

VkResult Engine::createInstance() {

    VkApplicationInfo appInfo {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "vulkan-learn",
        .apiVersion = VK_API_VERSION_1_3
    };

    uint32_t instanceExtensionsCount = 0;
    const char** instanceExtensions = glfwGetRequiredInstanceExtensions(&instanceExtensionsCount);

    VkInstanceCreateInfo instanceCI {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledExtensionCount = instanceExtensionsCount,
        .ppEnabledExtensionNames = instanceExtensions
    };

    return vkCreateInstance(&instanceCI, nullptr, &instance);

}