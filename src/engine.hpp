#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vulkan/vulkan.h>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstring>

#ifdef NDBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

class Engine {

    public:

        void run() {

            initWindow();
            initVulkan();
            loop();
            cleanup();

        }

        const uint32_t WINDOW_WIDTH = 1080;
        const uint32_t WINDOW_HEIGHT = 720;

    private:

        GLFWwindow* window;
        VkInstance instance;

        const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
        
        VkDebugUtilsMessengerEXT debugMessenger;

        static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallBack(
            VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            VkDebugUtilsMessageTypeFlagsEXT messageTypem,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            void* pUserData
        ) {

            std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
            return VK_FALSE;

        }

        // main
        void initWindow();
        void initVulkan();
        void loop();
        void cleanup();

        // vulkan init helpers
        void createInstance();
        bool checkValidationLayerSupport();
        std::vector<const char*> getRequiredExtentions();

        // debug messenger setup stuff
        void setupDebugMessenger();
        void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

};