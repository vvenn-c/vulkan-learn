#include <cstdint>
#include <vector>
#include <string>

#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>
#include <SDL3/SDL.h>
#include <shaderc/shaderc.hpp>

class Core {

public:

    Core(const char* windowName, uint32_t windowWidth, uint32_t windowHeight):
        windowName_(windowName),
        windowWidth_(windowWidth),
        windowHeight_(windowHeight) 
    {};

    void run();

private:

    constexpr static uint32_t vulkanVersion_ = VK_API_VERSION_1_4;

    // window stuff
    SDL_Window* window_ = nullptr;
    const char* windowName_;
    uint32_t windowWidth_, windowHeight_;
    bool quit = false;

    // vulkan core stuff
    VkSurfaceKHR surface_ = nullptr;
    VkInstance vulkanInstance_ = nullptr;
    VkPhysicalDevice physicalDevice_ = nullptr;
    VkDevice logicalDevice_ = nullptr;
    VmaAllocator vmaAllocator_ = nullptr;

    // queue stuff
    uint32_t graphicsQueueFamilyIndex_ = UINT32_MAX;
    VkQueue graphicsQueue_ = nullptr;

    // swapchain stuff
    VkSwapchainKHR swapchain_ = nullptr;
    uint32_t swapchainWidth_, swapchainHeight_;
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    std::vector<VkSemaphore> renderCompleteSemaphores_;
    bool requireSwapchainRecreate_ = false;

    VkImage depthImage_ = nullptr;
    VkImageView depthImageView_ = nullptr;
    VmaAllocation depthImageAllocation_ = nullptr;

    constexpr static VkFormat swapchainFormat_ = VK_FORMAT_B8G8R8A8_SRGB;
    constexpr static VkFormat depthImageFormat_ = VK_FORMAT_D32_SFLOAT;

    VkShaderModule vertexShader_ = nullptr;
    VkShaderModule fragmentShader_ = nullptr;

    VkPipelineLayout pipelineLayout_ = nullptr;
    VkPipeline pipeline_ = nullptr;

    struct FrameResources {
        VkCommandPool commandPool = nullptr;
        VkCommandBuffer commandBuffer = nullptr;
        VkSemaphore imageAcquiredSemaphore = nullptr;
    };

    constexpr static uint32_t maxFramesInFlight_ = 2;
    uint32_t frameIndex_ = 0;
    uint64_t nextSignalValue_ = maxFramesInFlight_ + 1;

    VkSemaphore timelineSemaphore_ = nullptr;
    std::array<FrameResources, maxFramesInFlight_> frameResources_;

    // main
    void initWindow();
    void initVulkan();
    void loop();
    void cleanup();

    void render();

    // vulkan init helpers
    bool createInstance();
    bool createSurface();
    bool findPhysicalDevice();
    bool checkFormatSupported();
    bool findGraphicsQueueIndex();
    bool createLogicalDevice();
    bool initVMA();
    bool createSwapchain();
    bool createShaders();
    bool createSyncResources();
    bool createCommandBuffers();

    VkShaderModule createShaderModule(const std::string &fileName, shaderc_shader_kind kind);
    VkPipeline createGraphicsPipeline();

    void destroySwapchain();

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
        void *pUserData
    );

};