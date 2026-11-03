CXX := C:/msys64/ucrt64/bin/g++.exe
VULKAN_LIB := C:\VulkanSDK\1.4.363.1\Lib

CXXFLAGS := -std=c++26 -O2 \
	-IC:\VulkanSDK\1.4.363.1\Include

VULKAN_LIBS := $(VULKAN_LIB)/vulkan-1.lib
SYSTEM_LIBS := -lstdc++exp

SOURCES := $(wildcard src/*.cpp)
vulkan-learn.exe: $(SOURCES)
	@echo building...
	@$(CXX) $(CXXFLAGS) -o $@ $^ $(VULKAN_LIBS) $(SYSTEM_LIBS)