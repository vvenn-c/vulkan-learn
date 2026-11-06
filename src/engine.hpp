#include <cstdint>
#include <vector>

#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>
#include <SDL3/SDL.h>

class Engine {

public:

    Engine(const char* windowName, uint32_t windowWidth, uint32_t windowHeight):
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

    VkImage depthImage_ = nullptr;
    VkImageView depthImageView_ = nullptr;
    VmaAllocation depthImageAllocation_ = nullptr;

    constexpr static VkFormat swapchainFormat_ = VK_FORMAT_B8G8R8A8_SRGB;
    constexpr static VkFormat depthImageFormat_ = VK_FORMAT_D32_SFLOAT;

    // main
    void initWindow();
    void initVulkan();
    void loop();
    void cleanup();

    // vulkan init helpers
    bool createInstance();
    bool createSurface();
    bool findPhysicalDevice();
    bool checkFormatSupported();
    bool findGraphicsQueueIndex();
    bool createLogicalDevice();
    bool initVMA();
    bool createSwapchain();

    void destroySwapchain();

    // error handling stuff
    VkResult checkError(const VkResult result);
    VkResult checkError(const VkResult result, const char* errorString);

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
        void *pUserData
    );

};