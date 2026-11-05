#include <print>
#include <stdexcept>
#include <cstdio>
#include <vector>

#define VOLK_IMPLEMENTATION

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <Volk/volk.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include "engine.hpp"

void Engine::run() {

    initWindow();
    initVulkan();
    loop();
    cleanup();

}

void Engine::initVulkan() {

    if (createInstance() != VK_SUCCESS) 
        throw std::runtime_error("failed to create vulkan instance");

    if (!createSurface())
        throw std::runtime_error("failed to create surface");

}

void Engine::initWindow() {

    if (SDL_InitSubSystem(SDL_INIT_VIDEO)) {

        window = SDL_CreateWindow(
            windowName, 
            windowWidth, windowHeight, 
            SDL_WINDOW_VULKAN |
            SDL_WINDOW_RESIZABLE
        );

    }

    if (!window) throw std::runtime_error("failed to create SDL window");

}

void Engine::loop() {

}

void Engine::cleanup() {

    if (vulkanInstance) vkDestroyInstance(vulkanInstance, nullptr);
    volkFinalize();

    if (window) SDL_DestroyWindow(window);
    SDL_Quit();

}

VkResult Engine::createInstance() {

    VkApplicationInfo appInfo {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = windowName,
        .apiVersion = vulkanVersion
    };

    uint32_t extensionCount = 0;
    char const* const* instanceExtensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    std::vector<const char*> requestedExtensions = { VK_EXT_DEBUG_UTILS_EXTENSION_NAME };

    for (int i = 0; i < extensionCount; i++)
        requestedExtensions.push_back(instanceExtensions[i]);

    std::vector<const char*> requestedLayers = { 
        "VK_LAYER_KHRONOS_validation" 
    };

    VkDebugUtilsMessengerCreateInfoEXT debugInfo {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = debugCallback
    };

    VkInstanceCreateInfo instanceCI {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = &debugInfo,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = static_cast<uint32_t>(requestedLayers.size()),
        .ppEnabledLayerNames = requestedLayers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(requestedExtensions.size()),
        .ppEnabledExtensionNames = requestedExtensions.data()
    };

    VkResult result = checkError(vkCreateInstance(&instanceCI, nullptr, &vulkanInstance));
    volkLoadInstance(vulkanInstance);

    return result;

}

bool Engine::createSurface() {

    return SDL_Vulkan_CreateSurface(window, vulkanInstance, nullptr, &surface);

}

VkResult Engine::checkError(const VkResult result) {

    if (result < VK_SUCCESS) {

        throw std::runtime_error(
            std::format(
                "vulkan function returned an error: {}",
                string_VkResult(result)
            )
        );

    }

    if (result > VK_SUCCESS) {

        std::println(
            stderr,
            "vulkan function returned status: {}",
            string_VkResult(result)
        );
        
    }

    return result;

}

VKAPI_ATTR VkBool32 VKAPI_CALL Engine::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData
) {

    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        std::println(stderr, "validation layer: {}", pCallbackData->pMessage);

    return VK_FALSE;

}