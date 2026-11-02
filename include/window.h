#pragma once

#include <SDL3/SDL_video.h>

namespace CityExplorer {
	extern SDL_Window* g_window;

	bool initSDL();
	void shutdownSDL();
}
