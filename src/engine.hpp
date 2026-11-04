#include <string>

#include <vulkan/vulkan.h>

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

    VkInstance instance_;

    template<auto Fn>
    VkResult checkError(VkResult result);

};