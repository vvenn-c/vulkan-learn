CXX := g++.exe

VULKAN := C:/VulkanSDK/1.4.363.1

CXXFLAGS := -std=c++26 -O2 -DVK_NO_PROTOTYPES \
	-I$(VULKAN)/Include \
	-L$(VULKAN)/Lib

VULKAN_LIBS := -lvulkan-1
SDL_LIBS := -lSDL3
SYSTEM_LIBS := -lstdc++exp

vulkan-learn.exe: $(wildcard src/*.cpp) $(wildcard src/*.hpp)
	@echo building...
	@$(CXX) $(CXXFLAGS) -o $@ $(wildcard src/*.cpp) $(VULKAN_LIBS) $(SDL_LIBS) $(SYSTEM_LIBS)
	@echo done!