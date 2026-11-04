CXX := g++.exe

VULKAN := C:/VulkanSDK/1.4.363.1

CXXFLAGS := -std=c++26 -O2 -freflection \
	-I$(VULKAN)/Include \
	-L$(VULKAN)/Lib

VULKAN_LIBS := -lvulkan-1
SYSTEM_LIBS := -lstdc++exp

vulkan-learn.exe: $(wildcard src/*.cpp) $(wildcard src/*.hpp)
	@echo building...
	@$(CXX) $(CXXFLAGS) -o $@ $(wildcard src/*.cpp) $(VULKAN_LIBS) $(SYSTEM_LIBS)
	@echo done!