#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>

class Engine {

public:

    Engine(const char* windowName, uint32_t windowWidth, uint32_t windowHeight):
        windowName(windowName),
        windowWidth(windowWidth),
        windowHeight(windowHeight) 
    {};

    void run();

    const char* windowName;
    uint32_t windowWidth, windowHeight;

private:

    constexpr static uint32_t vulkanVersion = VK_API_VERSION_1_4;

    SDL_Window* window = nullptr;
    VkSurfaceKHR surface = nullptr;
    VkInstance vulkanInstance = nullptr;
    VkPhysicalDevice physicalDevice = nullptr;

    uint32_t graphicsQueueFamilyIndex = UINT32_MAX;
    VkQueue graphicsQueue = nullptr;

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

    // error handling stuff
    VkResult checkError(const VkResult result);

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
        void *pUserData
    );

};