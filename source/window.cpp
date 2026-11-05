#include "window.h"

#include <print>
#include <format>
#include <iostream>

#include <SDL3/SDL.h>

SDL_Window* CityExplorer::g_window = nullptr;

static void setSDLMetaData(const char* name, const char* defineName, const char* value) {
	if (!SDL_SetAppMetadataProperty(name, value)) {
		std::string errorMsg = std::format("Could not set metadata property {}. Error {}", defineName, SDL_GetError());
		if (!SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Metadata Error", errorMsg.c_str(), nullptr))
			std::println(std::cerr, "{}", errorMsg);
	}
}

bool CityExplorer::initSDL() {
	if (g_window) {
#ifdef BUILD_NDIST
		std::println("Tried to initialize SDL which was already initialized, did you forget to destroy first?");
#endif
		// return true as SDL was already successfully initialized
		return true;
	}

#define setSDLMetaData(_name, _value) setSDLMetaData((_name), #_name, (_value))

	setSDLMetaData(SDL_PROP_APP_METADATA_NAME_STRING, "CityExplorer");
	setSDLMetaData(SDL_PROP_APP_METADATA_VERSION_STRING, "1.0.0");
	setSDLMetaData(SDL_PROP_APP_METADATA_IDENTIFIER_STRING, "com.github.thundersphun.CityExplorer");
	setSDLMetaData(SDL_PROP_APP_METADATA_CREATOR_STRING, "ThunderSphun");
	setSDLMetaData(SDL_PROP_APP_METADATA_URL_STRING, "https://github.com/ThunderSphun/CityExplorer");
	setSDLMetaData(SDL_PROP_APP_METADATA_TYPE_STRING, "game");

#undef setSDLMetaData

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
		std::string errorMsg = std::format("Could not initialize SDL\n{}", SDL_GetError());
		if (!SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Initialize error", errorMsg.c_str(), nullptr))
			std::println(std::cerr, "{}", errorMsg);
		return false;
	}

	float displayScale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
	if (displayScale == 0.0f) {
#ifdef BUILD_DEBUG
		std::string errorMsg = std::format("Could not query display content scale\n{}", SDL_GetError());
		if (!SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "DisplayContentScale error", errorMsg.c_str(), nullptr))
			std::println(std::cerr, "{}", errorMsg);
#endif
		displayScale = 1.0f;
	}

#ifdef BUILD_DEBUG
	std::println("Using display scale of {}", displayScale);
#endif

	g_window = SDL_CreateWindow("CityExplorer", 1280 * displayScale, 720 * displayScale, SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN);
	if (!g_window) {
		std::string errorMsg = std::format("Could not create a window. Error {}", SDL_GetError());
		if (!SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Initialize error", errorMsg.c_str(), nullptr))
			std::println(std::cerr, "{}", errorMsg);

		SDL_Quit();
		return false;
	}

	{
		const char* videoDriver = SDL_GetCurrentVideoDriver();
		if (videoDriver && strcmp(videoDriver, "wayland") != 0 &&
			!SDL_SetWindowPosition(g_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED)) {
#ifdef BUILD_DEBUG
			std::string errorMsg = std::format("Could not set window position\n{}", SDL_GetError());
			if (!SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Window reposition error", errorMsg.c_str(), nullptr))
				std::println(std::cerr, "{}", errorMsg);
#endif
		}
	}
	if (!SDL_ShowWindow(g_window)) {
		std::string errorMsg = std::format("Could not show window\n{}", SDL_GetError());
		if (!SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Window error", errorMsg.c_str(), nullptr))
			std::println(std::cerr, "{}", errorMsg);
		SDL_DestroyWindow(g_window);
		g_window = nullptr;
		SDL_Quit();
		return false;
	}

	return true;
}

void CityExplorer::shutdownSDL() {
	SDL_DestroyWindow(g_window);
	g_window = nullptr;

	SDL_Quit();
}
