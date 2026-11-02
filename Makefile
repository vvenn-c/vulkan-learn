CXX := C:/msys64/ucrt64/bin/g++.exe
VULKAN_LIB := C:\VulkanSDK\1.4.363.0\Lib

CXXFLAGS := -std=c++23 -O2 \
	-IC:\VulkanSDK\1.4.363.0\Include

VULKAN_LIBS := $(VULKAN_LIB)/vulkan-1.lib
GLFW_LIB := C:/msys64/ucrt64/lib/libglfw3.a
SYSTEM_LIBS := -lopengl32 -lgdi32 -luser32 -lshell32 -lwinmm

SOURCES := $(wildcard src/*.cpp)
vulkan-learn.exe: $(SOURCES)
	@echo building...
	@$(CXX) $(CXXFLAGS) -o $@ $^ $(VULKAN_LIBS) $(GLFW_LIB) $(SYSTEM_LIBS)