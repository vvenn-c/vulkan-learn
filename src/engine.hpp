#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vulkan/vulkan.h>

#include <iostream>

class Engine {

    public:

        void run();

        uint32_t WINDOW_WIDTH = 1080;
        uint32_t WINDOW_HEIGHT = 720;

    private:

        GLFWwindow* window;
        VkInstance instance;

        // main
        void initWindow();
        void initVulkan();
        void loop();
        void cleanup();

        // initVulkan() helpers
        VkResult createInstance();

};