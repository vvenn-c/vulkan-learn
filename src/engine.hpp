#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>

class Engine {

public:

    const char* windowName;
    uint32_t windowWidth, windowHeight;

    Engine(const char* windowName, uint32_t windowWidth, uint32_t windowHeight):
        windowName(windowName),
        windowWidth(windowWidth),
        windowHeight(windowHeight) 
    {};

    void run();

private:

    void initVulkan();
    void initWindow();
    void loop();
    void cleanup();

    SDL_Window* window = nullptr;
    VkSurfaceKHR surface = nullptr;
    VkInstance vulkanInstance = nullptr;

    constexpr static uint32_t vulkanVersion = VK_API_VERSION_1_4;

    VkResult createInstance();
    bool createSurface();
    VkPhysicalDevice findPhysicalDevice();

    // error handling
    VkResult checkError(const VkResult result);

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
        void *pUserData
    );

};