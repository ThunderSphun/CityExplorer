#include <iostream>
#include <format>
#include <SDL3/SDL.h>
#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
#include "stb_perlin.h"

static VkInstance gInstance = VK_NULL_HANDLE;

int main(int argc, char* argv[]) {
	std::cout << "Hello, World!" << std::endl;

	// sdl test
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
		std::cerr << "Error: SDL_Init(): " << SDL_GetError() << std::endl;
	else
		SDL_Quit();

	glm::vec4 glmTest{0, 1, 2, 3};
	std::cout << glm::to_string(glmTest) << std::endl;

	ImVec2 imguiTest = {1, 2};
	std::cout << std::format("ImVec2({}, {})", imguiTest[0], imguiTest[1]) << std::endl;

	std::cout << stb_perlin_noise3(0, 0, 0, 0, 0, 0) << std::endl;

#ifdef BUILD_DEBUG
	std::cout << "BUILD_DEBUG is set" << std::endl;
#endif
#ifdef BUILD_NDEBUG
	std::cout << "BUILD_NDEBUG is set" << std::endl;
#endif
#ifdef BUILD_RELEASE
	std::cout << "BUILD_RELEASE is set" << std::endl;
#endif
#ifdef BUILD_NRELEASE
	std::cout << "BUILD_NRELEASE is set" << std::endl;
#endif
#ifdef BUILD_DEBINFO
	std::cout << "BUILD_DEBINFO is set" << std::endl;
#endif
#ifdef BUILD_NDEBINFO
	std::cout << "BUILD_NDEBINFO is set" << std::endl;
#endif

	return 0;
}

#define STB_PERLIN_IMPLEMENTATION
#include "stb_perlin.h"