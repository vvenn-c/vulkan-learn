#include <print>
#include <stdexcept>
#include <cstdio>
#include <vector>

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

#define VOLK_IMPLEMENTATION
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

    if (!createInstance()) 
        throw std::runtime_error("failed to create vulkan instance");

    if (!createSurface())
        throw std::runtime_error("failed to create surface");

    if (!findPhysicalDevice())
        throw std::runtime_error("could not find appropriate physical device");

    if (!findGraphicsQueueIndex())
        throw std::runtime_error("could not find compatible graphics queue");

    if (!createLogicalDevice())
        throw std::runtime_error("could not create logical device");

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

bool Engine::createInstance() {

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

    if (result != VK_SUCCESS) return false;
    else return true;

}

bool Engine::createSurface() {

    return SDL_Vulkan_CreateSurface(window, vulkanInstance, nullptr, &surface);

}

// just gets the first discrete GPU it finds
// else return the first GPU it finds
bool Engine::findPhysicalDevice() {

    uint32_t physicalDeviceCount = 0;
    vkEnumeratePhysicalDevices(vulkanInstance, &physicalDeviceCount, nullptr);

    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    vkEnumeratePhysicalDevices(vulkanInstance, &physicalDeviceCount, physicalDevices.data());

    if (physicalDeviceCount > 0) {

        for (auto &device : physicalDevices) {

            VkPhysicalDeviceProperties deviceProperties{};
            vkGetPhysicalDeviceProperties(device, &deviceProperties);

            if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {

                Engine::physicalDevice = device;
                return true;

            }

        }

        Engine::physicalDevice = physicalDevices[0];

    }

    return false;

}

bool Engine::findGraphicsQueueIndex() {

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyCount, nullptr);

    // initialize a vector of size queueFamilyCount, and fill each element with a structure
    // with the appropriate sType "QUEUE_FAMILY_PROPERTIES_2"
    std::vector<VkQueueFamilyProperties2> queueFamilyProperties(queueFamilyCount,
        { .sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2 }
    );

    vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyCount, queueFamilyProperties.data());

    for (int currentFamilyIndex = 0; currentFamilyIndex < queueFamilyProperties.size(); currentFamilyIndex++) {

        VkBool32 hasPresentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, currentFamilyIndex, surface, &hasPresentSupport);

        const auto &properties = queueFamilyProperties[currentFamilyIndex];
        if (properties.queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT && hasPresentSupport) {

            Engine::graphicsQueueFamilyIndex = currentFamilyIndex;
            return true;

        }

    }

    return false;

}

bool Engine::createLogicalDevice() {

    // query supported features
    // this linked list is made specifically to check if certain features are available.
    // this list won't be used for actually creating the logical device
    VkPhysicalDeviceVulkan14Features supportedFeatures14 {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES,
        .pNext = nullptr
    };

    VkPhysicalDeviceVulkan13Features supportedFeatures13 {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &supportedFeatures14
    };

    VkPhysicalDeviceVulkan12Features supportedFeatures12 {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &supportedFeatures13
    };

    VkPhysicalDeviceFeatures2 supportedFeatures {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &supportedFeatures12
    };

    if (
        !supportedFeatures13.dynamicRendering || 
        !supportedFeatures13.synchronization2 ||
        !supportedFeatures12.timelineSemaphore
    ) {

        std::println(stderr, "physical device does not meet the feature requirements");
        return false;

    }

    // this list is the one actually being used to create the logical device
    VkPhysicalDeviceVulkan14Features features14 {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES,
        .pNext = nullptr
    };

    VkPhysicalDeviceVulkan13Features features13 {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &features14,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE
    };

    VkPhysicalDeviceVulkan12Features features12 {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &features13,
        .timelineSemaphore = VK_TRUE
    };

    VkPhysicalDeviceFeatures2 features {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &features12
    };

    std::vector<float> queuePriorities = { 1.0f };
    VkDeviceQueueCreateInfo graphicsQueueCI {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = graphicsQueueFamilyIndex,
        .queueCount = 1,
        .pQueuePriorities = queuePriorities.data()
    };

    const std::vector<const char*> deviceExtensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDeviceCreateInfo deviceCI {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &features,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &graphicsQueueCI,
        .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
        .pEnabledFeatures = nullptr // features struct is set in pNext (for some reason ????)
    };

    VkResult result = checkError(
        vkCreateDevice(physicalDevice, &deviceCI, nullptr, &logicalDevice)
    );

    if (result != VK_SUCCESS) return false;

    vkGetDeviceQueue(logicalDevice, graphicsQueueFamilyIndex, 0, &graphicsQueue);
    if (!graphicsQueue) {

        std::println(stderr, "could not get the graphics queue");
        return false;

    }

    return true;

}

VkResult Engine::checkError(const VkResult result) {

    if (result != VK_SUCCESS) {

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