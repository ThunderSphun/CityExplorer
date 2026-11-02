#include <print>
#include <iostream>

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
		if (videoDriver && strcmp(videoDriver, "wayland") == 0)
			i++;
	}
}

static void shutdown() {
	CityExplorer::shutdownVulkan();
	CityExplorer::shutdownSDL();
}

int main(int argc, char* argv[]) {
	if (!init())
		return -1;
	
	run();
	shutdown();

	return 0;
}
