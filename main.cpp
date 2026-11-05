#include <print>
#include <iostream>
#include <filesystem>
#include <fstream>

#include <SDL3/SDL.h>

#include "window.h"
#include "graphics.h"

static bool init() {
	if (!CityExplorer::initSDL()) {
		std::println(std::cerr, "Could not initialize SDL3");
		goto cleanup;
	}

	if (!CityExplorer::initVulkan()) {
		std::println(std::cerr, "Could not initialize Vulkan");
		goto cleanup_SDL;
	}

	return true;

cleanup_Vulkan:
	CityExplorer::shutdownVulkan();

cleanup_SDL:
	CityExplorer::shutdownSDL();

cleanup:
	return false;
}

static void run() {
	int i = 0;
	const char* videoDriver = SDL_GetCurrentVideoDriver();
	bool isWayland = videoDriver && strcmp(videoDriver, "wayland") == 0;
	while(i < 120) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_QUIT)
				i = 120;
			if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(CityExplorer::g_window))
				i = 120;
		}
		// run for 120 frames on wayland
		// dirty fix to have the application be able to be closed easily
		// wayland only shows a window when the first buffer is submitted
		i += isWayland;
	}
}

static void shutdown() {
	CityExplorer::shutdownVulkan();
	CityExplorer::shutdownSDL();
}

int main(int argc, char* argv[]) {
	// temporary file loading / pwd check
	{
		std::ifstream file;

		file.open(std::filesystem::path("assets") / "shaders" / "shader.spv", std::ios::ate | std::ios::binary);
		if (file) {
			std::vector<char> content(file.tellg());
			file.seekg(0);
			file.read(content.data(), static_cast<std::streamsize>(content.size()));
			for (int i = 0; i < content.size(); i++) {
				if (i != 0) {
					if (i % 8 == 0)
						std::print("    ");
					if (i % 16 == 0)
						std::print("\n");
				}
				std::print("{:02X} ", content[i]);
			}
			std::println();
		} else
			std::println("{}", std::filesystem::current_path().string());
		file.close();
	}

	for (int i = 0; i < 2; i++) {
		if (!init())
			return -1;

		run();
		shutdown();
	}

	return 0;
}
