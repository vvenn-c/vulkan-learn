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

    if (!createSyncResources())
        throw std::runtime_error("could not create sync related resources");

    if (!createCommandBuffers())
        throw std::runtime_error("could not create command buffer objects");

}

void Core::loop() {

    while (!quit) {

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            
            if (event.type == SDL_EVENT_QUIT)
                quit = true;

        }

        render();

    }

}

void Core::render() {

    if (requireSwapchainRecreate_) {

        vkDeviceWaitIdle(logicalDevice_);
        destroySwapchain();
        createSwapchain();
        requireSwapchainRecreate_ = false;

    }

    const uint32_t frameResIndex = frameIndex_++ % maxFramesInFlight_;
    const uint64_t signalValue = nextSignalValue_++;
    const uint64_t waitValue = signalValue - maxFramesInFlight_;

    VkSemaphoreWaitInfo waitInfo {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
        .semaphoreCount = 1,
        .pSemaphores = &timelineSemaphore_,
        .pValues = &waitValue
    };

    vkWaitSemaphores(logicalDevice_, &waitInfo, UINT64_MAX);

    FrameResources &res = frameResources_[frameResIndex];
    vkResetCommandPool(logicalDevice_, res.commandPool, 0);

    VkSemaphore imageAcquireSemaphore = frameResources_[frameResIndex].imageAcquiredSemaphore;

    uint32_t imageIndex = 0;
    VkResult acquireResult = vkAcquireNextImageKHR(
        logicalDevice_, swapchain_, UINT64_MAX, 
        imageAcquireSemaphore, VK_NULL_HANDLE, &imageIndex
    );

    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {

        requireSwapchainRecreate_ = true;
        return;

    } else if (acquireResult == VK_SUBOPTIMAL_KHR)
        requireSwapchainRecreate_ = true;

    VkCommandBufferBeginInfo commandBeginInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    vkBeginCommandBuffer(res.commandBuffer, &commandBeginInfo);

    std::vector<VkImageMemoryBarrier2> layoutBarriers {
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = 0,
            .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .image = swapchainImages_[imageIndex],
            .subresourceRange {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        },
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
            .srcAccessMask = 0,
            .dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                            VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
            .dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .image = depthImage_,
            .subresourceRange {
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        }
    };

    VkDependencyInfo dependencyInfo {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = static_cast<uint32_t>(layoutBarriers.size()),
        .pImageMemoryBarriers = layoutBarriers.data()
    };
    vkCmdPipelineBarrier2(res.commandBuffer, &dependencyInfo);

    VkRenderingAttachmentInfo colorAttachInfo {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = swapchainImageViews_[imageIndex],
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue{ .color{0.01f, 0.01f, 0.01f, 1} }
    };

    VkRenderingAttachmentInfo depthAttachInfo {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = depthImageView_,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .clearValue{ .depthStencil{1.0f, 0} }
    };

    VkRenderingInfo renderingInfo {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea {
            .offset{ .x = 0, .y = 0 },
            .extent{ .width = swapchainWidth_, .height = swapchainHeight_ }
        },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachInfo,
        .pDepthAttachment = &depthAttachInfo
    };

    vkCmdBeginRendering(res.commandBuffer, &renderingInfo); {

        VkViewport viewport {
            .x = 0, .y = 0,
            .width = static_cast<float>(swapchainWidth_),
            .height = static_cast<float>(swapchainHeight_)
        };
        vkCmdSetViewport(res.commandBuffer, 0, 1, &viewport);

        VkRect2D scissor {
            .offset { .x = 0, .y = 0 },
            .extent { .width = swapchainWidth_, .height = swapchainHeight_ }
        };
        vkCmdSetScissor(res.commandBuffer, 0, 1, &scissor);

        vkCmdBindPipeline(res.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
        vkCmdDraw(res.commandBuffer, 3, 1, 0, 0);
        
    } vkCmdEndRendering(res.commandBuffer);

    VkImageMemoryBarrier2 presentLayoutBarrier {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_NONE,
        .dstAccessMask = 0,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .image = swapchainImages_[imageIndex],
        .subresourceRange {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    VkDependencyInfo presentDependencyInfo {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &presentLayoutBarrier
    };

    vkCmdPipelineBarrier2(res.commandBuffer, &presentDependencyInfo);
    vkEndCommandBuffer(res.commandBuffer);

    VkSemaphoreSubmitInfo imageAcquireWaitInfo {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = imageAcquireSemaphore,
        .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
    };

    std::vector<VkSemaphoreSubmitInfo> semaphoreSignals {
        {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = renderCompleteSemaphores_[imageIndex],
            .stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT
        },
        {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = timelineSemaphore_,
            .value = signalValue,
            .stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT
        }
    };

    VkCommandBufferSubmitInfo commandSubmitInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = res.commandBuffer
    };

    VkSubmitInfo2 submitInfo {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount = 1,
        .pWaitSemaphoreInfos = &imageAcquireWaitInfo,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &commandSubmitInfo,
        .signalSemaphoreInfoCount = static_cast<uint32_t>(semaphoreSignals.size()),
        .pSignalSemaphoreInfos = semaphoreSignals.data()
    };
    vkQueueSubmit2(graphicsQueue_, 1, &submitInfo, VK_NULL_HANDLE);

    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &renderCompleteSemaphores_[imageIndex],
        .swapchainCount = 1,
        .pSwapchains = &swapchain_,
        .pImageIndices = &imageIndex,
        .pResults = nullptr
    };

    vkQueuePresentKHR(graphicsQueue_, &presentInfo);
    
}

void Core::cleanup() {

    vkDeviceWaitIdle(logicalDevice_);

    if (timelineSemaphore_) vkDestroySemaphore(logicalDevice_, timelineSemaphore_, nullptr);

    for (auto &res : frameResources_) {

        vkDestroySemaphore(logicalDevice_, res.imageAcquiredSemaphore, nullptr);
        vkDestroyCommandPool(logicalDevice_, res.commandPool, nullptr);

    }

    if (pipelineLayout_) vkDestroyPipelineLayout(logicalDevice_, pipelineLayout_, nullptr);

    if (pipeline_) vkDestroyPipeline(logicalDevice_, pipeline_, nullptr);

    if (vertexShader_) vkDestroyShaderModule(logicalDevice_, vertexShader_, nullptr);
    if (fragmentShader_) vkDestroyShaderModule(logicalDevice_, fragmentShader_, nullptr);

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
        .pNext = nullptr,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = static_cast<uint32_t>(requestedLayers.size()),
        .ppEnabledLayerNames = requestedLayers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(requestedExtensions.size()),
        .ppEnabledExtensionNames = requestedExtensions.data()
    };

    volkInitialize();

    VkResult result = checkError(vkCreateInstance(&instanceCI, nullptr, &vulkanInstance_));
    if (result != VK_SUCCESS) return false;
    else {

        volkLoadInstance(vulkanInstance_);
        return true;

    }

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

    vkGetPhysicalDeviceFeatures2(physicalDevice_, &supportedFeatures);

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
        .ppEnabledExtensionNames = deviceExtensions.data(),
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
            "failed to create swapchain image view"
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
            .height = swapchainHeight_,
            .depth = 1
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

    std::println("{} compiled successfully.", fileName);
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

bool Core::createSyncResources() {

    VkSemaphoreTypeCreateInfo semaphoreTypeCI {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
        .initialValue = maxFramesInFlight_
    };

    VkSemaphoreCreateInfo semaphoreCI {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &semaphoreTypeCI
    };

    VkResult result = checkError(
        vkCreateSemaphore(logicalDevice_, &semaphoreCI, nullptr, &timelineSemaphore_),
        "could not create timeline semaphore"
    ); if (result != VK_SUCCESS) return false;

    for (FrameResources &res : frameResources_) {

        VkSemaphoreCreateInfo semaphoreCI {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
        };

        result = checkError(
            vkCreateSemaphore(logicalDevice_, &semaphoreCI, nullptr, &res.imageAcquiredSemaphore),
            "could not create per-frame image-aquire semaphore"
        ); if (result != VK_SUCCESS) return false;

    }

    return true;

}

bool Core::createCommandBuffers() {

    for (FrameResources &res : frameResources_) {

        VkCommandPoolCreateInfo poolCI {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .queueFamilyIndex = graphicsQueueFamilyIndex_
        };

        VkResult result = checkError(
            vkCreateCommandPool(logicalDevice_, &poolCI, nullptr, &res.commandPool),
            "could not create command buffer pool"
        ); if (result != VK_SUCCESS) return false;

        VkCommandBufferAllocateInfo commandAllocateInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = res.commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };

        result = checkError(
            vkAllocateCommandBuffers(logicalDevice_, &commandAllocateInfo, &res.commandBuffer),
            "could not allocate command buffer"
        ); if (result != VK_SUCCESS) return false;

    }

    return true;

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