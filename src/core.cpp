#include <print>
#include <stdexcept>
#include <cstdio>
#include <vector>
#include <string>

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

#define VOLK_IMPLEMENTATION
#include <Volk/volk.h>

#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include "shaderc/shaderc.hpp"

#include "core.hpp"
#include "utils.hpp"

void Core::run() {

    initWindow();
    initVulkan();
    loop();
    cleanup();

}

void Core::initWindow() {

    if (SDL_InitSubSystem(SDL_INIT_VIDEO)) {

        window_ = SDL_CreateWindow(
            windowName_, 
            windowWidth_, windowHeight_, 
            SDL_WINDOW_VULKAN |
            SDL_WINDOW_RESIZABLE
        );

    }

    if (!window_) throw std::runtime_error("failed to create SDL window");

}

void Core::initVulkan() {

    if (!createInstance()) 
        throw std::runtime_error("failed to create vulkan instance");

    if (!createSurface())
        throw std::runtime_error("failed to create surface");

    if (!findPhysicalDevice())
        throw std::runtime_error("could not find appropriate physical device");

    if (!checkFormatSupported())
        throw std::runtime_error("requested swapchain format is not supported by the surface");

    if (!findGraphicsQueueIndex())
        throw std::runtime_error("could not find compatible graphics queue");

    if (!createLogicalDevice())
        throw std::runtime_error("could not create logical device");

    if (!initVMA())
        throw std::runtime_error("could not initialize VMA");

    if (!createSwapchain())
        throw std::runtime_error("could not create swapchain");
    
    if (!createShaders())
        throw std::runtime_error("could not create shader modules");

    if (pipeline_ = createGraphicsPipeline(); !pipeline_)
        throw std::runtime_error("could not create graphics pipeline");

}

void Core::loop() {

}

void Core::cleanup() {

    destroySwapchain();

    if (vmaAllocator_) vmaDestroyAllocator(vmaAllocator_);

    if (surface_) vkDestroySurfaceKHR(vulkanInstance_, surface_, nullptr);

    if (logicalDevice_) vkDestroyDevice(logicalDevice_, nullptr);

    if (vulkanInstance_) vkDestroyInstance(vulkanInstance_, nullptr);
    volkFinalize();

    if (window_) SDL_DestroyWindow(window_);
    SDL_Quit();

}

bool Core::createInstance() {

    VkApplicationInfo appInfo {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = windowName_,
        .apiVersion = vulkanVersion_
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

    VkResult result = checkError(vkCreateInstance(&instanceCI, nullptr, &vulkanInstance_));
    volkLoadInstance(vulkanInstance_);

    if (result != VK_SUCCESS) return false;
    else return true;

}

bool Core::createSurface() {

    return SDL_Vulkan_CreateSurface(window_, vulkanInstance_, nullptr, &surface_);

}

// just gets the first discrete GPU it finds
// else return the first GPU it finds
bool Core::findPhysicalDevice() {

    uint32_t physicalDeviceCount = 0;
    vkEnumeratePhysicalDevices(vulkanInstance_, &physicalDeviceCount, nullptr);

    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    vkEnumeratePhysicalDevices(vulkanInstance_, &physicalDeviceCount, physicalDevices.data());

    if (physicalDeviceCount > 0) {

        for (auto &device : physicalDevices) {

            VkPhysicalDeviceProperties deviceProperties{};
            vkGetPhysicalDeviceProperties(device, &deviceProperties);

            if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {

                Core::physicalDevice_ = device;
                return true;

            }

        }

        Core::physicalDevice_ = physicalDevices[0];

    }

    return false;

}

bool Core::checkFormatSupported() {

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, nullptr);

    std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, surfaceFormats.data());

    for (const VkSurfaceFormatKHR &surfaceFormat : surfaceFormats) {

        if (surfaceFormat.format == swapchainFormat_)
            return true;

    }

    return false;

}

bool Core::findGraphicsQueueIndex() {

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice_, &queueFamilyCount, nullptr);

    // initialize a vector of size queueFamilyCount, and fill each element with a structure
    // with the appropriate sType "QUEUE_FAMILY_PROPERTIES_2"
    std::vector<VkQueueFamilyProperties2> queueFamilyProperties(queueFamilyCount,
        { .sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2 }
    );

    vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice_, &queueFamilyCount, queueFamilyProperties.data());

    for (int currentFamilyIndex = 0; currentFamilyIndex < queueFamilyProperties.size(); currentFamilyIndex++) {

        VkBool32 hasPresentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice_, currentFamilyIndex, surface_, &hasPresentSupport);

        const auto &properties = queueFamilyProperties[currentFamilyIndex];
        if (properties.queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT && hasPresentSupport) {

            Core::graphicsQueueFamilyIndex_ = currentFamilyIndex;
            return true;

        }

    }

    return false;

}

bool Core::createLogicalDevice() {

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
        .queueFamilyIndex = graphicsQueueFamilyIndex_,
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
        vkCreateDevice(physicalDevice_, &deviceCI, nullptr, &logicalDevice_)
    );

    if (result != VK_SUCCESS) return false;

    vkGetDeviceQueue(logicalDevice_, graphicsQueueFamilyIndex_, 0, &graphicsQueue_);
    if (!graphicsQueue_) {

        std::println(stderr, "could not get the graphics queue");
        return false;

    }

    return true;

}

bool Core::initVMA() {

    VmaVulkanFunctions vmaFunctionsInfo{};

    VmaAllocatorCreateInfo vmaAllocatorCI {
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = physicalDevice_,
        .device = logicalDevice_,
        .pVulkanFunctions = &vmaFunctionsInfo,
        .instance = vulkanInstance_,
        .vulkanApiVersion = vulkanVersion_
    };

    VkResult result = checkError(vmaImportVulkanFunctionsFromVolk(&vmaAllocatorCI, &vmaFunctionsInfo));
    if (result != VK_SUCCESS) return false;

    result = checkError(vmaCreateAllocator(&vmaAllocatorCI, &vmaAllocator_));
    if (result != VK_SUCCESS) return false;
    else return true;

}

bool Core::createSwapchain() {

    swapchainWidth_ = windowWidth_;
    swapchainHeight_ = windowHeight_;

    VkSurfaceCapabilitiesKHR surfaceCaps{};

    VkResult result = 
        checkError(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, surface_, &surfaceCaps));
    if (result != VK_SUCCESS) return false;

    // with a minimum of 2, get at least a minimum number of images as
    // specified by the surface capabilities
    uint32_t requestedImageCount = std::max(2u, surfaceCaps.minImageCount);

    // zero means unlimited image count. 
    // if the maximum image count is some number greater than zero,
    // get the lesser number of images.
    if (surfaceCaps.maxImageCount > 0)
        requestedImageCount = std::min(requestedImageCount, surfaceCaps.maxImageCount);

    // basically this makes sure we get at least 2, or as few as possible,
    // as specified by the surface capabilities and no more.

    VkSwapchainCreateInfoKHR swapchainCI {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface_,
        .minImageCount = requestedImageCount,
        .imageFormat = swapchainFormat_,
        .imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
        .imageExtent {
            .width = swapchainWidth_,
            .height = swapchainHeight_
        },
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform = surfaceCaps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR
    };

    result = checkError(
        vkCreateSwapchainKHR(logicalDevice_, &swapchainCI, nullptr, &swapchain_)
    ); if (result != VK_SUCCESS) return false;
    
    uint32_t imageCount = 0;
    vkGetSwapchainImagesKHR(logicalDevice_, swapchain_, &imageCount, nullptr);

    swapchainImages_.resize(imageCount);
    vkGetSwapchainImagesKHR(logicalDevice_, swapchain_, &imageCount, swapchainImages_.data());
    swapchainImageViews_.resize(imageCount);

    for (size_t i = 0; i < swapchainImages_.size(); i++) {

        VkImageViewCreateInfo imageViewCI {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = swapchainImages_[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = swapchainFormat_,
            .subresourceRange {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            },
        };

        result = checkError(
            vkCreateImageView(logicalDevice_, &imageViewCI, nullptr, &swapchainImageViews_[i]),
            "failed to create image view"
        ); if (result != VK_SUCCESS) return false;
        
    }

    renderCompleteSemaphores_.resize(swapchainImages_.size());
    for (VkSemaphore &semaphore : renderCompleteSemaphores_) {

        VkSemaphoreCreateInfo semaphoreCI {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
        };

        result = checkError(
            vkCreateSemaphore(logicalDevice_, &semaphoreCI, nullptr, &semaphore),
            "failed to create semaphore"
        ); if (result != VK_SUCCESS) return false;
        
    }

    VkImageCreateInfo depthImageCI {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = depthImageFormat_,
        .extent {
            .width = swapchainWidth_,
            .height = swapchainHeight_
        },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VmaAllocationCreateInfo allocationCI {
        .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO
    };

    result = checkError(
        vmaCreateImage(vmaAllocator_, &depthImageCI, &allocationCI, &depthImage_, &depthImageAllocation_, nullptr),
        "failed to create depth image"
    ); if (result != VK_SUCCESS) return false;

    VkImageViewCreateInfo depthImageViewCI {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = depthImage_,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = depthImageFormat_,
        .subresourceRange {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .levelCount = 1,
            .layerCount = 1
        },
    };

    result = checkError(
        vkCreateImageView(logicalDevice_, &depthImageViewCI, nullptr, &depthImageView_),
        "failed to create depth image view"
    );

    if (result != VK_SUCCESS) return false;
    else return true;

}

bool Core::createShaders() {

    if (vertexShader_ = createShaderModule("shader.vert", shaderc_vertex_shader); !vertexShader_)
        return false;

    if (fragmentShader_ = createShaderModule("shader.frag", shaderc_fragment_shader); !fragmentShader_)
        return false;

    return true;

}

VkShaderModule Core::createShaderModule(const std::string &fileName, shaderc_shader_kind kind) {

    const std::string shaderPath = "src/shaders/" + fileName;
    const std::string source = readTextFile(shaderPath);
    if (source.empty()) {

        std::println(stderr, "could not find shader file named {}", fileName);
        return nullptr;

    }

    std::println("compiling shader: {}", shaderPath);
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;

    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_4);
    options.SetTargetSpirv(shaderc_spirv_version_1_6);
    options.SetOptimizationLevel(shaderc_optimization_level_performance);

    shaderc::CompilationResult compResult = 
        compiler.CompileGlslToSpv(source, kind, fileName.c_str(), options);

    if (compResult.GetCompilationStatus() != shaderc_compilation_status_success) {

        std::println(stderr, "shader compilation status: {}", compResult.GetErrorMessage());
        return nullptr;

    }

    const size_t shaderSize = (compResult.cend() - compResult.cbegin()) * sizeof(uint32_t);
    VkShaderModuleCreateInfo shaderModuleCI {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = shaderSize,
        .pCode = compResult.cbegin()
    };

    VkShaderModule shaderModule = nullptr;
    VkResult result = checkError(
        vkCreateShaderModule(logicalDevice_, &shaderModuleCI, nullptr, &shaderModule),
        "could not create shader module"
    ); if (result != VK_SUCCESS) return nullptr;

    return shaderModule;

}

VkPipeline Core::createGraphicsPipeline() {

    VkPipelineLayoutCreateInfo pipelineLayoutCI {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 0,
        .pushConstantRangeCount = 0
    };

    VkResult result = checkError(
        vkCreatePipelineLayout(logicalDevice_, &pipelineLayoutCI, nullptr, &pipelineLayout_),
        "could not create pipeline layout"
    ); if (result != VK_SUCCESS) return nullptr;

    const char* entryPoint = "main";
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertexShader_,
            .pName = entryPoint
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragmentShader_,
            .pName = entryPoint
        }
    };

    VkPipelineVertexInputStateCreateInfo vertexInputCI {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyCI {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };

    VkPipelineDepthStencilStateCreateInfo depthStencilCI {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .stencilTestEnable = VK_FALSE
    };

    VkPipelineViewportStateCreateInfo viewportCI {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = nullptr,
        .scissorCount = 1,
        .pScissors = nullptr
    };

    // backface culling !!!!!!!!!!!!!
    VkPipelineRasterizationStateCreateInfo rasterCI {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.0f
    };

    VkPipelineMultisampleStateCreateInfo multiSampleCI {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };

    VkPipelineColorBlendAttachmentState attachState {
        .blendEnable = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                          VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT |
                          VK_COLOR_COMPONENT_A_BIT
    };

    VkPipelineColorBlendStateCreateInfo blendCI {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &attachState
    };

    std::vector<VkDynamicState> dynamicState {
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicStateCI {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = static_cast<uint32_t>(dynamicState.size()),
        .pDynamicStates = dynamicState.data()
    };

    VkPipelineRenderingCreateInfo renderCI {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &swapchainFormat_,
        .depthAttachmentFormat = depthImageFormat_
    };

    VkGraphicsPipelineCreateInfo pipelineCI {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &renderCI,
        .stageCount = static_cast<uint32_t>(shaderStages.size()),
        .pStages = shaderStages.data(),
        .pVertexInputState = &vertexInputCI,
        .pInputAssemblyState = &inputAssemblyCI,
        .pViewportState = &viewportCI,
        .pRasterizationState = &rasterCI,
        .pMultisampleState = &multiSampleCI,
        .pDepthStencilState = &depthStencilCI,
        .pColorBlendState = &blendCI,
        .pDynamicState = &dynamicStateCI,
        .layout = pipelineLayout_,
        .renderPass = VK_NULL_HANDLE
    };

    VkPipeline newPipeline;
    result = checkError(
        vkCreateGraphicsPipelines(logicalDevice_, nullptr, 1, &pipelineCI, nullptr, &newPipeline)
    ); if (result != VK_SUCCESS) return nullptr;

    return newPipeline;

}

void Core::destroySwapchain() {

    // destroy image views
    for (VkImageView &swapchainImageView : swapchainImageViews_)
        vkDestroyImageView(logicalDevice_, swapchainImageView, nullptr);
    swapchainImageViews_.clear();

    // destroy semaphores
    for (VkSemaphore &semaphore : renderCompleteSemaphores_)
        vkDestroySemaphore(logicalDevice_, semaphore, nullptr);
    renderCompleteSemaphores_.clear();

    if (swapchain_) {

        vkDestroySwapchainKHR(logicalDevice_, swapchain_, nullptr);
        swapchain_ = nullptr;

    }

    if (depthImageView_) {

        vkDestroyImageView(logicalDevice_, depthImageView_, nullptr);
        vmaDestroyImage(vmaAllocator_, depthImage_, depthImageAllocation_);
        depthImageView_ = nullptr;
        
    }

}

VKAPI_ATTR VkBool32 VKAPI_CALL Core::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData
) {

    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        std::println(stderr, "validation layer: {}", pCallbackData->pMessage);

    return VK_FALSE;

}