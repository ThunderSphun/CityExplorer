#include <SDL3/SDL.h>
#include <vulkan/vulkan.hpp>
#include <SDL3/SDL_vulkan.h>

#include <print>
#include <format>
#include <iostream>
#include <expected>
#include <algorithm>

SDL_Window* window;
vk::Instance instance;
#ifdef BUILD_NRELEASE
vk::DebugUtilsMessengerEXT debugMessenger;
vk::PhysicalDevice physicalDevice;

VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
	vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
	vk::DebugUtilsMessageTypeFlagsEXT type,
	const vk::DebugUtilsMessengerCallbackDataEXT* callbackData,
	void* userData) {
#ifdef BUILD_NDEBUG
	if (severity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
#endif
	std::println(std::cerr, "Validation layer: type {} msg: {}", vk::to_string(type), callbackData->pMessage);

	return vk::False;
}
#endif

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
	VULKAN_HPP_DEFAULT_DISPATCHER.init();
	{
		auto extensionProperties = vk::enumerateInstanceExtensionProperties();
		if (!extensionProperties) {
			std::println(std::cerr, "Could not load instance extension properties");
			goto cleanup;
		}

#ifdef BUILD_DEBUG
		for (const auto& extension : *extensionProperties)
			std::println("Got available extension property {}", extension.extensionName.data());
#endif

		std::vector<const char*> extensions;
#ifdef BUILD_NRELEASE
		extensions.emplace_back(vk::EXTDebugUtilsExtensionName);
#endif

		uint32_t sdlExtensionCount = 0;
		const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);
		extensions.reserve(sdlExtensionCount);

		for (uint32_t i = 0; i < sdlExtensionCount; i++) {
#ifdef BUILD_DEBUG
			std::println("checking extension property {} required by SDL", sdlExtensions[i]);
#endif
			if (std::none_of(
					extensionProperties->begin(), extensionProperties->end(),
					[extension = sdlExtensions[i]](auto const& extensionProperty) {
						return strcmp(extensionProperty.extensionName, extension) == 0;
					}
				)
			) {
				std::println(std::cerr, "Vulkan does not have the required extension property {} required by SDL", sdlExtensions[i]);
				return false;
			} else
				extensions.emplace_back(sdlExtensions[i]);
		}

		const vk::ApplicationInfo appInfo{
			.pApplicationName   = "CityExplorer",
			.applicationVersion = 1,
			.pEngineName        = "No Engine",
			.engineVersion      = 1,
			.apiVersion         = vk::ApiVersion14,
		};

#ifdef BUILD_NRELEASE
		auto layerProperties = vk::enumerateInstanceLayerProperties();
		if (!layerProperties) {
			std::println(std::cerr, "Could not load instance layer properties");
			goto cleanup;
		}

#ifdef BUILD_DEBUG
		for (const auto& layer : *layerProperties)
			std::println("Got available layer property {}", layer.layerName.data());
#endif
		const char* layers[] = {
			"VK_LAYER_KHRONOS_validation",
		};

		for (uint32_t i = 0; i < sizeof(layers) / sizeof(layers[0]); i++) {
#ifdef BUILD_DEBUG
			std::println("checking layer property {}", layers[i]);
#endif
			if (std::none_of(
					layerProperties->begin(), layerProperties->end(),
					[layer = layers[i]](auto const& layerProperty) {
						return strcmp(layerProperty.layerName, layer) == 0;
					}
				)
			) {
				std::println(std::cerr, "Vulkan does not have the required layer property {}", layers[i]);
				goto cleanup;
			}
		}
#endif // BUILD_NRELEASE

		vk::InstanceCreateInfo createInfo{
			.pApplicationInfo = &appInfo,
#ifdef BUILD_NRELEASE
			.enabledLayerCount = sizeof(layers) / sizeof(layers[0]),
			.ppEnabledLayerNames = layers,
#endif
			.enabledExtensionCount = (uint32_t) extensions.size(),
			.ppEnabledExtensionNames = extensions.data(),
		};
		std::expected<vk::Instance, vk::Result> _instance = vk::createInstance(createInfo);
		if (_instance)
			instance = *_instance;
		else if (_instance.error() == vk::Result::eErrorIncompatibleDriver) {
			// this is an attempt to fix incompatibility between vulkan and macOS moltenVK
			extensions.emplace_back(vk::KHRPortabilityEnumerationExtensionName);

			vk::InstanceCreateInfo createInfo{
				.flags = vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR,
				.pApplicationInfo = &appInfo,
#ifdef BUILD_NRELEASE
				.enabledLayerCount = sizeof(layers) / sizeof(layers[0]),
				.ppEnabledLayerNames = layers,
#endif
				.enabledExtensionCount = (uint32_t) extensions.size(),
				.ppEnabledExtensionNames = extensions.data(),
			};

			_instance = vk::createInstance(createInfo);
		}

		// try to assign again if the macOS fix was needed
		if (_instance && !instance)
			instance = *_instance;
		else if (!_instance) {
			std::println(std::cerr, "Could not create vulkan instance {}", vk::to_string(_instance.error()));
			goto cleanup_instance;
		}
		VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);
	}

	{
#ifdef BUILD_NRELEASE
		vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
#ifdef BUILD_DEBUG
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
#endif
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
		);
		vk::DebugUtilsMessageTypeFlagsEXT typeFlags(
#ifdef BUILD_DEBUG
			vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
#endif
			vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
			vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
		);
		vk::DebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo{
			.messageSeverity = severityFlags,
			.messageType = typeFlags,
			.pfnUserCallback = &debugCallback,
		};

		std::expected<vk::DebugUtilsMessengerEXT, vk::Result> _debugMessenger = instance.createDebugUtilsMessengerEXT(debugMessengerCreateInfo);
		if (_debugMessenger)
			debugMessenger = *_debugMessenger;
		else
			std::println(std::cerr, "Could not create a debug messenger, continuing without");
#endif
	}

	{
		auto physicalDevices = instance.enumeratePhysicalDevices();
		if (!physicalDevices || physicalDevices->empty()) {
			std::println(std::cerr, "Failed to find GPU with Vulkan support");
			goto cleanup_debugMessenger;
		}

		std::vector<const char*> requiredExtensions{
			vk::KHRSwapchainExtensionName,
		};

		for (const vk::PhysicalDevice& pd : *physicalDevices) {
			if (physicalDevice && physicalDevice.getProperties2().properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
				break;

			// if a device has already been chosen,
			// don't evaluate the other ones if it is not a dedicated gpu
			if (physicalDevice)
				continue;

#ifdef BUILD_DEBUG
			std::println("checking physical device {}", pd.getProperties2().properties.deviceName.data());
#endif

#ifdef BUILD_DEBUG
			std::println("  checking api version");
#endif
			if (pd.getProperties2().properties.apiVersion < vk::ApiVersion13)
				continue;

#ifdef BUILD_DEBUG
			std::println("  checking queueFamilies");
#endif
			std::vector<vk::QueueFamilyProperties2> queueFamilies = pd.getQueueFamilyProperties2();
			if (std::none_of(queueFamilies.begin(), queueFamilies.end(),
					[](const vk::QueueFamilyProperties2& queueFamily) {
						return (queueFamily.queueFamilyProperties.queueFlags & vk::QueueFlagBits::eGraphics) == vk::QueueFlagBits::eGraphics;
					}))
				continue;

#ifdef BUILD_DEBUG
			std::println("  checking device extensions");
#endif
			auto deviceExtensions = pd.enumerateDeviceExtensionProperties();
			if (!deviceExtensions)
				continue;

#ifdef BUILD_DEBUG
			for (const auto& deviceExtension : *deviceExtensions)
				std::println("    {}", deviceExtension.extensionName.data());
#endif

			if (!std::all_of(requiredExtensions.begin(), requiredExtensions.end(),
					[deviceExtensions](const auto& requiredExtension) {
						return std::any_of(deviceExtensions->begin(), deviceExtensions->end(),
							[requiredExtension](const auto& deviceExtension) {
								return std::strcmp(deviceExtension.extensionName, requiredExtension) == 0;
							});
					}))
				continue;

			physicalDevice = pd;
		}

		if (!physicalDevice) {
			std::println(std::cerr, "Could not find any suitable device");
			goto cleanup_debugMessenger;
		}

#ifdef BUILD_NRELEASE
			std::println("Found physical device {} of type {}",
				physicalDevice.getProperties2().properties.deviceName.data(),
				vk::to_string(physicalDevice.getProperties2().properties.deviceType));
#endif
	}

	return true;

cleanup_physicalDevice:
	physicalDevice = nullptr;

cleanup_debugMessenger:
	if (debugMessenger)
		instance.destroyDebugUtilsMessengerEXT(debugMessenger);
	debugMessenger = nullptr;

cleanup_instance:
	instance.destroy();
	instance = nullptr;

cleanup:
	return false;
}

void shutdownSDL() {
	SDL_DestroyWindow(window);
	window = nullptr;

	SDL_Quit();
}

void shutdownVulkan() {
	physicalDevice = nullptr;

	if (debugMessenger)
		instance.destroyDebugUtilsMessengerEXT(debugMessenger);
	debugMessenger = nullptr;

	instance.destroy();
	instance = nullptr;
}

bool init() {
	if (!initSDL()) {
		std::println(std::cerr, "Could not initialize SDL3");
		goto cleanup;
	}

	if (!initVulkan()) {
		std::println(std::cerr, "Could not initialize Vulkan");
		goto cleanup_SDL;
	}

	return true;

cleanup_Vulkan:
	shutdownVulkan();

cleanup_SDL:
	shutdownSDL();

cleanup:
	return false;
}

void run() {
	int i = 0;
	const char* videoDriver = SDL_GetCurrentVideoDriver();
	while(i < 120) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_QUIT)
				i = 120;
			if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window))
				i = 120;
		}
		// run for 120 frames on wayland
		// dirty fix to have the application be able to be closed easily
		// wayland only shows a window when the first buffer is submitted
		if (videoDriver && strcmp(videoDriver, "wayland") == 0)
			i++;
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

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
