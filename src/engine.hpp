#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

class Engine {

    public:

        void run();

        const char* appName;
        uint32_t WINDOW_WIDTH, WINDOW_HEIGHT;

        Engine(const char* appName, uint32_t WINDOW_WIDTH, uint32_t WINDOW_HEIGHT):
            appName(appName),
            WINDOW_WIDTH(WINDOW_WIDTH),
            WINDOW_HEIGHT(WINDOW_HEIGHT) {};

    private:

        GLFWwindow* window_;
        VkInstance instance_;
        VkDevice device_;
        VkQueue queue_;

        // main
        void initWindow();
        void initVulkan();
        void loop();
        void cleanup();

        // initVulkan() helpers
        VkResult createInstance();
        VkResult selectDevice(uint32_t deviceIndex);

};