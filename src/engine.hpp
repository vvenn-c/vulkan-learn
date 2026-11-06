#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>

class Engine {

public:

    Engine(const char* windowName, uint32_t windowWidth, uint32_t windowHeight):
        windowName_(windowName),
        windowWidth_(windowWidth),
        windowHeight_(windowHeight) 
    {};

    void run();

    const char* windowName_;
    uint32_t windowWidth_, windowHeight_;

private:

    constexpr static uint32_t vulkanVersion_ = VK_API_VERSION_1_4;

    SDL_Window* window_ = nullptr;
    VkSurfaceKHR surface_ = nullptr;
    VkInstance vulkanInstance_ = nullptr;
    VkPhysicalDevice physicalDevice_ = nullptr;
    VkDevice logicalDevice_ = nullptr;

    uint32_t graphicsQueueFamilyIndex_ = UINT32_MAX;
    VkQueue graphicsQueue_ = nullptr;

    // main
    void initVulkan();
    void initWindow();
    void loop();
    void cleanup();

    // vulkan init helpers
    bool createInstance();
    bool createSurface();
    bool findPhysicalDevice();
    bool findGraphicsQueueIndex();
    bool createLogicalDevice();

    // error handling stuff
    VkResult checkError(const VkResult result);

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
        void *pUserData
    );

};