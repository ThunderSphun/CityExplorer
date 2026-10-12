#include <SDL3/SDL.h>

#include <print>
#include <iostream>

int main(int argc, char* argv[]) {
#define setSDLMetaData(_name, _value) \
if (!SDL_SetAppMetadataProperty(_name, _value)) { \
	std::println(std::cerr, "Could not set metadata property {}. Error {}", #_name, SDL_GetError()); \
	return -1; \
}

	setSDLMetaData(SDL_PROP_APP_METADATA_NAME_STRING, "CityExplorer");
	setSDLMetaData(SDL_PROP_APP_METADATA_VERSION_STRING, "1.0.0");
	setSDLMetaData(SDL_PROP_APP_METADATA_IDENTIFIER_STRING, "com.github.thundersphun.CityExplorer");
	setSDLMetaData(SDL_PROP_APP_METADATA_CREATOR_STRING, "ThunderSphun");
	setSDLMetaData(SDL_PROP_APP_METADATA_URL_STRING, "https://github.com/ThunderSphun/CityExplorer");
	setSDLMetaData(SDL_PROP_APP_METADATA_TYPE_STRING, "game");

#undef setSDLMetaData

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
		std::println(std::cerr, "Could not initialize SDL. Error {}", SDL_GetError());
		return -1;
	}

	SDL_Window* window = SDL_CreateWindow("CityExplorer", 1920, 1080, SDL_WINDOW_RESIZABLE);
	if (!window) {
		std::println(std::cerr, "Could not create a window. Error {}", SDL_GetError());
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
