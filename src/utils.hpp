#pragma once

#include <print>
#include <fstream>
#include <sstream>
#include <string>

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

inline VkResult checkError(const VkResult result) {

    if (result != VK_SUCCESS) {

        std::println(
            stderr,
            "vulkan function returned status: {}",
            string_VkResult(result)
        );
        
    }

    return result;

}

inline VkResult checkError(const VkResult result, const char* errorString) {

    if (result != VK_SUCCESS) {

        std::print(stderr, "{} | ", errorString);
        std::println(
            stderr,
            "vulkan function returned status: {}",
            string_VkResult(result)
        );
        
    }

    return result;

}

inline std::string readTextFile(const std::string &filePath) {

    std::ifstream infile(filePath);

    if (infile.is_open()) {

        std::stringstream buffer;
        buffer << infile.rdbuf();
        const std::string output = buffer.str();
        infile.close();
        return output;

    }

    return std::string();

}