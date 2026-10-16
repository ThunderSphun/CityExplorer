#include <SDL3/SDL.h>
#include <vulkan/vulkan.hpp>
#include <SDL3/SDL_vulkan.h>

#include <print>
#include <format>
#include <iostream>
#include <expected>

SDL_Window* window;
vk::Instance instance;

bool initSDL() {
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

	SDL_Window* window = SDL_CreateWindow("CityExplorer", 1280 * displayScale, 720 * displayScale, SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN);
	if (!window) {
		std::string errorMsg = std::format("Could not create a window. Error {}", SDL_GetError());
		if (!SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Initialize error", errorMsg.c_str(), nullptr))
			std::println(std::cerr, "{}", errorMsg);

		SDL_Quit();
		return false;
	}

	{
		const char* videoDriver = SDL_GetCurrentVideoDriver();
		if (videoDriver && strcmp(videoDriver, "wayland") != 0 &&
			!SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED)) {
	#ifdef BUILD_DEBUG
			std::string errorMsg = std::format("Could not set window position\n{}", SDL_GetError());
			if (!SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Window reposition error", errorMsg.c_str(), nullptr))
				std::println(std::cerr, "{}", errorMsg);
	#endif
		}
	}
	if (!SDL_ShowWindow(window)) {
		std::string errorMsg = std::format("Could not show window\n{}", SDL_GetError());
		if (!SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Window error", errorMsg.c_str(), nullptr))
			std::println(std::cerr, "{}", errorMsg);
		SDL_Quit();
		return false;
	}

	return true;
}

bool initVulkan() {
	constexpr vk::ApplicationInfo appInfo{
		.pApplicationName   = "CityExplorer",
		.applicationVersion = 1,
		.pEngineName        = "No Engine",
		.engineVersion      = 1,
		.apiVersion         = vk::ApiVersion14,
	};

	uint32_t sdlExtensionCount = 0;
	const char* const* SDL_Extensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);
	auto extensionProperties = vk::enumerateInstanceExtensionProperties();
	if (!extensionProperties) {
		std::println(std::cerr, "Could not load instance extension properties");
		return false;
	}

	for (uint32_t i = 0; i < sdlExtensionCount; i++) {
#ifdef BUILD_DEBUG
		std::println("checking extension property {} required by SDL", SDL_Extensions[i]);
#endif
		if (std::ranges::none_of(
				*extensionProperties,
				[extension = SDL_Extensions[i]](auto const& extensionProperty) {
					return strcmp(extensionProperty.extensionName, extension) == 0;
				}
			)
		) {
			std::println(std::cerr, "Vulkan does not have the required extension property {} required by SDL", SDL_Extensions[i]);
			return false;
		}
	}
	vk::InstanceCreateInfo createInfo{
		.pApplicationInfo = &appInfo,
		.enabledExtensionCount = sdlExtensionCount,
		.ppEnabledExtensionNames = SDL_Extensions,
	};
	std::expected<vk::Instance, vk::Result> _instance = vk::createInstance(createInfo);
	if (_instance)
		instance = *_instance;
	else {
		std::println(std::cerr, "Could not create vulkan instance");
		return false;
	}

	return true;
}

void shutdownSDL() {
	SDL_DestroyWindow(window);
	window = nullptr;

	SDL_Quit();
}

void shutdownVulkan() {
	instance.destroy();
	instance = nullptr;
}

bool init() {
	if (!initSDL()) {
		std::println(std::cerr, "Could not initialize SDL3");
		goto shutdown;
	}

	if (!initVulkan()) {
		std::println(std::cerr, "Could not initialize Vulkan");
		goto shutdown_SDL;
	}

	return true;

shutdown_Vulkan:
	shutdownVulkan();

shutdown_SDL:
	shutdownSDL();

shutdown:
	return false;
}

void run() {
	for (int i = 0; i < 120; i++) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_QUIT)
				i = 120;
			if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window))
				i = 120;
		}
	}
}

void shutdown() {
	shutdownVulkan();
	shutdownSDL();
}

int main(int argc, char* argv[]) {
	if (!init())
		return -1;

	run();
	shutdown();

	return 0;
}
