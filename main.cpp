#include <SDL3/SDL.h>

#include <print>
#include <format>
#include <iostream>

int main(int argc, char* argv[]) {
#define setSDLMetaData(_name, _value) \
if (!SDL_SetAppMetadataProperty(_name, _value)) { \
	std::string errorMsg = std::format("Could not set metadata property {}. Error {}", #_name, SDL_GetError()); \
	if (!SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Metadata Error", errorMsg.c_str(), nullptr)) \
		std::println(std::cerr, "{}", errorMsg); \
}

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
		return -1;
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

	SDL_Window* window = SDL_CreateWindow("CityExplorer", 1280 * displayScale, 720 * displayScale, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);
	if (!window) {
		std::string errorMsg = std::format("Could not create a window. Error {}", SDL_GetError());
		if (!SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Initialize error", errorMsg.c_str(), nullptr))
			std::println(std::cerr, "{}", errorMsg);

		SDL_Quit();
		return -1;
	}

	if (!SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED)) {
#ifdef BUILD_DEBUG
		std::string errorMsg = std::format("Could not set window position\n{}", SDL_GetError());
		if (!SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Window reposition error", errorMsg.c_str(), nullptr))
			std::println(std::cerr, "{}", errorMsg);
#endif
	}
	if (!SDL_ShowWindow(window)) {
		std::string errorMsg = std::format("Could not show window\n{}", SDL_GetError());
		if (!SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Window error", errorMsg.c_str(), nullptr))
			std::println(std::cerr, "{}", errorMsg);
		SDL_Quit();
		return -1;
	}

	bool running = true;
	while (running) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_QUIT)
				running = false;
			if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window))
				running = false;
		}
	}

	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}
