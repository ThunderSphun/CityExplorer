#include <print>
#include <iostream>

#include <SDL3/SDL.h>

#include "platform.h"
#include "renderer.h"

static bool init();
static void run();
static void shutdown();

static bool init() {
	if (!CityExplorer::initSDL()) {
		std::println(std::cerr, "Could not initialize SDL3");
		goto cleanup;
	}

	if (!CityExplorer::initVulkan()) {
		std::println(std::cerr, "Could not initialize Vulkan");
		goto cleanup_SDL;
	}

	// All systems are initialized, now show window to user
	// If something goes wrong, shutdown every system
	if (!SDL_ShowWindow(CityExplorer::g_window)) {
		std::string errorMsg = std::format("Could not show window\n{}", SDL_GetError());
		if (!SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Window error", errorMsg.c_str(), nullptr))
			std::println(std::cerr, "{}", errorMsg);
		shutdown();
		return false;
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

		CityExplorer::drawFrame();
	}
}

static void shutdown() {
	CityExplorer::shutdownVulkan();
	CityExplorer::shutdownSDL();
}

int main(int argc, char* argv[]) {
	for (int i = 0; i < 2; i++) {
		if (!init())
			return -1;

		run();
		shutdown();
	}

	return 0;
}
