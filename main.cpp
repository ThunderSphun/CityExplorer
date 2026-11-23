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
	const char* videoDriver = SDL_GetCurrentVideoDriver();
	bool minimized = false;
	while(true) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_QUIT)
				return;
			if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(CityExplorer::g_window))
				return;
			if (event.type == SDL_EVENT_WINDOW_MINIMIZED)
				minimized = true;
			if (event.type == SDL_EVENT_WINDOW_RESTORED)
				minimized = false;
			if (event.type == SDL_EVENT_WINDOW_RESIZED || event.type == SDL_EVENT_WINDOW_ENTER_FULLSCREEN || event.type == SDL_EVENT_WINDOW_LEAVE_FULLSCREEN)
				CityExplorer::recreateSwapchain();

			if (event.type == SDL_EVENT_KEY_DOWN) {
				if (event.key.key == SDLK_1) {
					SDL_SetWindowFullscreenMode(CityExplorer::g_window, NULL);
					SDL_SetWindowFullscreen(CityExplorer::g_window, true);
				}
				if (event.key.key == SDLK_2)
					SDL_SetWindowFullscreen(CityExplorer::g_window, false);
				if (event.key.key == SDLK_3)
					SDL_MaximizeWindow(CityExplorer::g_window);
				if (event.key.key == SDLK_4)
					SDL_RestoreWindow(CityExplorer::g_window);
				if (event.key.key == SDLK_5) {
					SDL_MinimizeWindow(CityExplorer::g_window);
					minimized = true;
				}
			}
		}

		if (!minimized)
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
