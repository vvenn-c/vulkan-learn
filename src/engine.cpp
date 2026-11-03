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

    window_ = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, appName, nullptr, nullptr);

}

void Engine::initVulkan() {

    if (createInstance() != VK_SUCCESS)
        throw std::runtime_error("failed to create vulkan instance");

    if (selectDevice(0) != VK_SUCCESS)
        throw std::runtime_error("failed to select device");

}

void Engine::loop() {
    while (!glfwWindowShouldClose(window_)) {

        glfwPollEvents();

    }
}

void Engine::cleanup() {

    vkDestroyInstance(instance_, nullptr);

    glfwDestroyWindow(window_);
    glfwTerminate();

}

VkResult Engine::createInstance() {

    VkApplicationInfo appInfo {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = appName,
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

    return vkCreateInstance(&instanceCI, nullptr, &instance_);

}

// selects the physical device
// and creates a logical device to communicate with it
VkResult Engine::selectDevice(uint32_t deviceIndex) {

    VkResult result;

    // get device count
    uint32_t deviceCount = 0;
    result = vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);
    if (result != VK_SUCCESS) return result;

    // use that device count to create a appropriately sized vector
    // and then call the vkEnumerate function again to get the actual data and push to the vector
    std::vector<VkPhysicalDevice> devices(deviceCount);
    result = vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());
    if (result != VK_SUCCESS) return result;

    VkPhysicalDeviceProperties2 deviceProperties{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2
    };

    vkGetPhysicalDeviceProperties2(devices[deviceIndex], &deviceProperties);
    std::cout << "selected device: " << deviceProperties.properties.deviceName << std::endl;

    // get family of queues
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties2(devices[deviceIndex], &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties2> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties2(devices[deviceIndex], &queueFamilyCount, queueFamilies.data());

    // find the graphics queue with VK_QUEUE_GRAPHICS_BIT
    uint32_t queueFamily = 0;
    for (size_t i = 0; i < queueFamilies.size(); i++) {
        if (queueFamilies[i].queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT) {

            queueFamily = i;
            break;

        }
    }

    // check that the queue supports presentation
    if (!glfwGetPhysicalDevicePresentationSupport(instance_, devices[deviceIndex], queueFamily))
        return VK_ERROR_FEATURE_NOT_PRESENT;

    const float qfPriorities = 1.0f;
    VkDeviceQueueCreateInfo queueCI {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = queueFamily,
        .queueCount = 1,
        .pQueuePriorities = &qfPriorities
    };

    const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkPhysicalDeviceVulkan12Features enabledVk12Featrues {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .descriptorIndexing = true,
        .shaderSampledImageArrayNonUniformIndexing = true,
        .descriptorBindingVariableDescriptorCount = true,
        .runtimeDescriptorArray = true,
        .bufferDeviceAddress = true
    };

    VkPhysicalDeviceVulkan13Features enabledVk13Features {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &enabledVk12Featrues,
        .synchronization2 = true,
        .dynamicRendering = true
    };

    VkPhysicalDeviceFeatures enabledVk10Features {
        .samplerAnisotropy = VK_TRUE
    };

    VkDeviceCreateInfo deviceCI {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &enabledVk13Features,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCI,
        .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data(),
        .pEnabledFeatures = &enabledVk10Features
    };

    result = vkCreateDevice(devices[deviceIndex], &deviceCI, nullptr, &device_);
    if (result != VK_SUCCESS) return result;

    vkGetDeviceQueue(device_, queueFamily, 0, &queue_);

    return VK_SUCCESS;
    
}