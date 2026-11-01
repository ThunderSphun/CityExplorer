#include <SDL3/SDL.h>
#include <vulkan/vulkan.hpp>
#include <SDL3/SDL_vulkan.h>
#include <glm/glm.hpp>

#include <print>
#include <format>
#include <iostream>
#include <expected>
#include <algorithm>

SDL_Window* window;
vk::Instance instance;
#ifdef BUILD_NRELEASE
vk::DebugUtilsMessengerEXT debugMessenger;
vk::SurfaceKHR surface;
vk::PhysicalDevice physicalDevice;
vk::Device logicalDevice;
vk::Queue graphicsQueue;
vk::SwapchainKHR swapchain;
std::vector<vk::Image> images;
std::vector<vk::ImageView> imageViews;

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

	window = SDL_CreateWindow("CityExplorer", 1280 * displayScale, 720 * displayScale, SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN);
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
		SDL_DestroyWindow(window);
		window = nullptr;
		SDL_Quit();
		return false;
	}

	return true;
}

static bool createVulkanInstance() {
	const vk::ApplicationInfo appInfo{
		.pApplicationName = "CityExplorer",
		.applicationVersion = vk::makeApiVersion(0, 1, 0, 0),
		.pEngineName = "No Engine",
		.engineVersion = vk::makeApiVersion(0, 1, 0, 0),
		.apiVersion = vk::ApiVersion14,
	};

	std::expected<std::vector<vk::ExtensionProperties>, vk::Result> extensionProperties = vk::enumerateInstanceExtensionProperties();
	if (!extensionProperties) {
		std::println(std::cerr, "Could not load instance extension properties");
		return false;
	}

#ifdef BUILD_DEBUG
	for (const vk::ExtensionProperties& extension : *extensionProperties)
		std::println("Got available extension property {}", extension.extensionName.data());
#endif

	std::vector<const char*> extensions;
#ifdef BUILD_NRELEASE
	extensions.emplace_back(vk::EXTDebugUtilsExtensionName);
#endif

	uint32_t sdlExtensionCount = 0;
	const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);
	if (sdlExtensions == nullptr) {
		std::println(std::cerr, "Could not load required vulkan extensions from SDL {}", SDL_GetError());
		return false;
	}
	extensions.reserve(sdlExtensionCount);

	for (uint32_t i = 0; i < sdlExtensionCount; i++) {
#ifdef BUILD_DEBUG
		std::println("checking extension property {} required by SDL", sdlExtensions[i]);
#endif
		if (std::none_of(
			extensionProperties->begin(), extensionProperties->end(),
			[extension = sdlExtensions[i]](const vk::ExtensionProperties& extensionProperty) {
				return strcmp(extensionProperty.extensionName, extension) == 0;
		})) {
			std::println(std::cerr, "Vulkan does not have the required extension property {} required by SDL", sdlExtensions[i]);
			return false;
		} else
			extensions.emplace_back(sdlExtensions[i]);
	}

	std::expected<std::vector<vk::LayerProperties>, vk::Result> layerProperties = vk::enumerateInstanceLayerProperties();
	if (!layerProperties) {
		std::println(std::cerr, "Could not load instance layer properties");
		return false;
	}

#ifdef BUILD_DEBUG
	for (const vk::LayerProperties& layer : *layerProperties)
		std::println("Got available layer property {}", layer.layerName.data());
#endif

	const char* layers[] = {
#ifdef BUILD_NRELEASE
		"VK_LAYER_KHRONOS_validation",
#endif
	};

	for (uint32_t i = 0; i < sizeof(layers) / sizeof(layers[0]); i++) {
#ifdef BUILD_DEBUG
		std::println("checking layer property {}", layers[i]);
#endif
		if (std::none_of(
			layerProperties->begin(), layerProperties->end(),
			[layer = layers[i]](const vk::LayerProperties& layerProperty) {
				return strcmp(layerProperty.layerName, layer) == 0;
		})) {
			std::println(std::cerr, "Vulkan does not have the required layer property {}", layers[i]);
			return false;
		}
	}

	vk::InstanceCreateInfo createInfo{
		.pApplicationInfo = &appInfo,
		.enabledLayerCount = static_cast<uint32_t>(sizeof(layers) / sizeof(layers[0])),
		.ppEnabledLayerNames = layers,
		.enabledExtensionCount = (uint32_t) extensions.size(),
		.ppEnabledExtensionNames = extensions.data(),
	};

	std::expected<vk::Instance, vk::Result> _instance = vk::createInstance(createInfo)
		.or_else([createInfo, extensions](vk::Result result) mutable -> std::expected<vk::Instance, vk::Result> {
			if (result != vk::Result::eErrorIncompatibleDriver)
				return std::unexpected(result);

			// this is an attempt to fix incompatibility between vulkan and macOS moltenVK
			extensions.emplace_back(vk::KHRPortabilityEnumerationExtensionName);

			return vk::createInstance(createInfo
				.setEnabledExtensionCount(static_cast<uint32_t>(extensions.size()))
				.setPpEnabledExtensionNames(extensions.data())
			);
	});

	if (_instance)
		instance = *_instance;
	else {
		std::println(std::cerr, "Could not create vulkan instance {}", vk::to_string(_instance.error()));
		return false;
	}
	return true;
}

void setupDebugMessenger() {
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
		std::println(std::cerr, "Got error {} whilst creating debug messenger, continuing without", vk::to_string(_debugMessenger.error()));
#endif // BUILD_NRELEASE
}

bool createSurface() {
	VkSurfaceKHR _surface;
	if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &_surface)) {
		std::println(std::cerr, "SDL could not create a vulkan surface {}", SDL_GetError());
		return false;
	}

	surface = _surface;
	return true;
}

bool isDeviceSuitable(const vk::PhysicalDevice& device) {
	const std::vector<const char*> requiredExtensions{
		vk::KHRSwapchainExtensionName,
	};

#ifdef BUILD_NRELEASE
	std::println("checking physical device {}", device.getProperties2().properties.deviceName.data());
#endif

#ifdef BUILD_DEBUG
	std::println("  checking api version");
#endif
	if (device.getProperties2().properties.apiVersion < vk::ApiVersion13)
		return false;

#ifdef BUILD_DEBUG
	std::println("  checking queueFamilies");
#endif
	std::vector<vk::QueueFamilyProperties2> queueFamilies = device.getQueueFamilyProperties2();
	if (std::none_of(queueFamilies.begin(), queueFamilies.end(),
		[](const vk::QueueFamilyProperties2& queueFamily) {
		return (queueFamily.queueFamilyProperties.queueFlags & vk::QueueFlagBits::eGraphics) == vk::QueueFlagBits::eGraphics;
	}))
		return false;

#ifdef BUILD_DEBUG
	std::println("  checking device extensions");
#endif
	std::expected<std::vector<vk::ExtensionProperties>, vk::Result> deviceExtensions = device.enumerateDeviceExtensionProperties();
	if (!deviceExtensions)
		return false;

#ifdef BUILD_DEBUG
	for (const vk::ExtensionProperties& deviceExtension : *deviceExtensions)
		std::println("    {}", deviceExtension.extensionName.data());
#endif

	for (const char* requiredExtension : requiredExtensions) {
		if (!std::any_of(deviceExtensions->begin(), deviceExtensions->end(),
				[requiredExtension](const vk::ExtensionProperties& deviceExtension) {
				return std::strcmp(deviceExtension.extensionName, requiredExtension) == 0;
		}))
			return false;
	}

	auto features = device.getFeatures2<
		vk::PhysicalDeviceFeatures2,
		vk::PhysicalDeviceVulkan11Features,
		vk::PhysicalDeviceVulkan13Features,
		vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
	>();

	if (!features.get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters)
		return false;
	if (!features.get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering)
		return false;
	if (!features.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState)
		return false;

	return true;
}

bool loadPhysicalDevice() {
	std::expected<std::vector<vk::PhysicalDevice>, vk::Result> physicalDevices = instance.enumeratePhysicalDevices();
	if (!physicalDevices || physicalDevices->empty()) {
		std::println(std::cerr, "Failed to find GPU with Vulkan support");
		return false;
	}

	for (const vk::PhysicalDevice& pd : *physicalDevices) {
		if (physicalDevice && physicalDevice.getProperties2().properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
			break;

		// if a device has already been chosen,
		// don't evaluate the other ones if it is not a dedicated gpu
		if (physicalDevice)
			continue;

		if (!isDeviceSuitable(pd))
			continue;

		physicalDevice = pd;
	}

	if (!physicalDevice) {
		std::println(std::cerr, "Could not find any suitable device");
		return false;
	}

#ifdef BUILD_NRELEASE
	std::println("Found physical device {} of type {}",
		physicalDevice.getProperties2().properties.deviceName.data(),
		vk::to_string(physicalDevice.getProperties2().properties.deviceType));
#endif

	return true;
}

bool createLogicalDevice() {
	float queuePriority = 0.5f;
	vk::DeviceQueueCreateInfo deviceQueueCreateInfo{
		.queueCount = 1,
		.pQueuePriorities = &queuePriority,
	};
	{
		std::vector<vk::QueueFamilyProperties2> queueFamilyProperties = physicalDevice.getQueueFamilyProperties2();
		for (size_t i = 0; i < queueFamilyProperties.size(); i++) {
			if ((queueFamilyProperties[i].queueFamilyProperties.queueFlags & vk::QueueFlagBits::eGraphics) != vk::QueueFlagBits::eGraphics)
				continue;

			if (!physicalDevice.getSurfaceSupportKHR(i, surface))
				continue;

			deviceQueueCreateInfo.setQueueFamilyIndex(i);
			break;
		}

		if (deviceQueueCreateInfo.queueFamilyIndex == -1) {
			std::println(std::cerr, "Could not find a queue which supports both graphics and presenting to the window");
			return false;
		}
	}

	vk::StructureChain<
		vk::PhysicalDeviceFeatures2,
		vk::PhysicalDeviceVulkan11Features,
		vk::PhysicalDeviceVulkan13Features,
		vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
	> featureChain{
		{},
		{ .shaderDrawParameters = true, },
		{ .dynamicRendering = true, },
		{ .extendedDynamicState = true, },
	};

	std::vector<const char*> requiredExtensions{
		vk::KHRSwapchainExtensionName,
	};

	vk::DeviceCreateInfo deviceCreateInfo{
		.pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
		.queueCreateInfoCount = 1,
		.pQueueCreateInfos = &deviceQueueCreateInfo,
		.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size()),
		.ppEnabledExtensionNames = requiredExtensions.data(),
	};

	std::expected<vk::Device, vk::Result> _logicalDevice = physicalDevice.createDevice(deviceCreateInfo);
	if (_logicalDevice)
		logicalDevice = *_logicalDevice;
	else {
		std::println(std::cerr, "Got error {} whilst creating a logical device", vk::to_string(_logicalDevice.error()));
		return false;
	}

	graphicsQueue = logicalDevice.getQueue(deviceQueueCreateInfo.queueFamilyIndex, 0);

	return true;
}

bool createSwapChain() {
	std::expected<vk::SurfaceCapabilitiesKHR, vk::Result> surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
	if (!surfaceCapabilities) {
		std::println(std::cerr, "Could not load surface capabilities {}", vk::to_string(surfaceCapabilities.error()));
		return false;
	}

	vk::Extent2D extend;

	if (surfaceCapabilities->currentExtent.width == UINT32_MAX)
		extend = surfaceCapabilities->currentExtent;
	else {
		bool gotSize = SDL_GetWindowSizeInPixels(window, (int*) &extend.width, (int*) &extend.height);

		if (gotSize) {
			extend.width = glm::clamp(extend.width, surfaceCapabilities->minImageExtent.width, surfaceCapabilities->maxImageExtent.width);
			extend.height = glm::clamp(extend.height, surfaceCapabilities->minImageExtent.height, surfaceCapabilities->maxImageExtent.height);
		} else {
#ifdef BUILD_NRELEASE
			std::println(std::cerr, "Could not get SDL frame size, got error {}, using minimal size", SDL_GetError());
#endif
			extend = surfaceCapabilities->minImageExtent;
		}
	}

#ifdef BUILD_DEBUG
	std::println("Using surface extend of {{{}, {}}}", extend.width, extend.height);
#endif

	uint32_t imageCount = 3;
	if (imageCount < surfaceCapabilities->minImageCount)
		imageCount = surfaceCapabilities->minImageCount;

	if (surfaceCapabilities->maxImageCount != 0 && surfaceCapabilities->maxImageCount < imageCount)
		imageCount = surfaceCapabilities->maxImageCount;

#ifdef BUILD_DEBUG
	std::println("Using {} images in swapChain", imageCount);
#endif

	std::expected<std::vector<vk::SurfaceFormatKHR>, vk::Result> surfaceFormats = physicalDevice.getSurfaceFormatsKHR(surface);
	if (!surfaceFormats || surfaceFormats->empty()) {
		std::println(std::cerr, "Could not load surface formats {}", vk::to_string(surfaceFormats.error()));
		return false;
	}

	vk::SurfaceFormatKHR surfaceFormat = (*surfaceFormats)[0];
	for (vk::SurfaceFormatKHR& format : *surfaceFormats) {
		if (format.format != vk::Format::eR8G8B8A8Srgb)
			continue;

		if (format.colorSpace != vk::ColorSpaceKHR::eSrgbNonlinear)
			continue;

		surfaceFormat = format;
		break;
	}

#ifdef BUILD_DEBUG
	std::println("Chosen surface format {} and color space {}", vk::to_string(surfaceFormat.format), vk::to_string(surfaceFormat.colorSpace));
#endif

	std::expected<std::vector<vk::PresentModeKHR>, vk::Result> presentModes = physicalDevice.getSurfacePresentModesKHR(surface);
	if (!presentModes) {
		std::println(std::cerr, "Could not load surface formats {}", vk::to_string(presentModes.error()));
		return false;
	}
	if (std::none_of(presentModes->begin(), presentModes->end(), [](const vk::PresentModeKHR& mode) {
		return mode == vk::PresentModeKHR::eFifo;
	})) {
		std::println(std::cerr, "Default present mode {} is not available", vk::to_string(vk::PresentModeKHR::eFifo));
		return false;
	}

	vk::PresentModeKHR presentMode = vk::PresentModeKHR::eFifo;
	for (vk::PresentModeKHR& mode : *presentModes) {
		if (mode != vk::PresentModeKHR::eMailbox)
			continue;

		presentMode = mode;
		break;
	}
#ifdef BUILD_DEBUG
	std::println("Chosen present mode {}", vk::to_string(presentMode));
#endif

	vk::SwapchainCreateInfoKHR swapchainCreateInfo{
		.surface = surface,
		.minImageCount = imageCount,
		.imageFormat = surfaceFormat.format,
		.imageColorSpace = surfaceFormat.colorSpace,
		.imageExtent = extend,
		.imageArrayLayers = 1,
		.imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
		.imageSharingMode = vk::SharingMode::eExclusive,
		.preTransform = surfaceCapabilities->currentTransform,
		.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
		.presentMode = presentMode,
		.clipped = true,
	};

	std::expected<vk::SwapchainKHR, vk::Result> _swapchain = logicalDevice.createSwapchainKHR(swapchainCreateInfo);
	if (!_swapchain) {
		std::println(std::cerr, "Got error {} whilst creating the swapchain", vk::to_string(_swapchain.error()));
		return false;
	}
	swapchain = *_swapchain;

	std::expected<std::vector<vk::Image>, vk::Result> _images = logicalDevice.getSwapchainImagesKHR(swapchain);
	if (!_images) {
		std::println(std::cerr, "Got error {} whilst getting the swapchain images", vk::to_string(_images.error()));
		logicalDevice.destroySwapchainKHR(swapchain);
		swapchain = nullptr;
		return false;
	}
	images = *_images;

	imageViews.reserve(images.size());

	vk::ImageViewCreateInfo imageViewCreateInfo{
		.viewType = vk::ImageViewType::e2D,
		.format = surfaceFormat.format,
		.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 },
	};

	for (size_t i = 0; i < images.size(); i++) {
		imageViewCreateInfo.setImage(images[i]);
		std::expected<vk::ImageView, vk::Result> imageView = logicalDevice.createImageView(imageViewCreateInfo);
		if (!imageView) {
			std::println(std::cerr, "Got error {} whilst creating image view {}", vk::to_string(imageView.error()), i);
			for (vk::ImageView& imageView : imageViews)
				logicalDevice.destroyImageView(imageView);
			images.clear();
			images.shrink_to_fit();
			return false;
		}
		imageViews.emplace_back(*imageView);
	}

	return true;
}

bool initVulkan() {
	VULKAN_HPP_DEFAULT_DISPATCHER.init();

	if (!createVulkanInstance())
		goto cleanup;

	VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);

	setupDebugMessenger();

	if (!createSurface())
		goto cleanup_debugMessenger;

	if (!loadPhysicalDevice())
		goto cleanup_surface;


	if (!createLogicalDevice())
		goto cleanup_physicalDevice;

	VULKAN_HPP_DEFAULT_DISPATCHER.init(logicalDevice);

	if (!createSwapChain())
		goto cleanup_logicalDevice;

	return true;

cleanup_swapChain:
	for (vk::ImageView& imageView : imageViews)
		logicalDevice.destroyImageView(imageView);
	imageViews.clear();
	imageViews.shrink_to_fit();
	images.clear();
	images.shrink_to_fit();
	logicalDevice.destroySwapchainKHR(swapchain);
	swapchain = nullptr;

cleanup_logicalDevice:
	graphicsQueue = nullptr;

	logicalDevice.destroy();
	logicalDevice = nullptr;

cleanup_physicalDevice:
	physicalDevice = nullptr;

cleanup_surface:
	SDL_Vulkan_DestroySurface(instance, surface, nullptr);
	surface = nullptr;

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
	for (vk::ImageView& imageView : imageViews)
		logicalDevice.destroyImageView(imageView);
	imageViews.clear();
	imageViews.shrink_to_fit();
	images.clear();
	images.shrink_to_fit();
	logicalDevice.destroySwapchainKHR(swapchain);
	swapchain = nullptr;

	graphicsQueue = nullptr;

	logicalDevice.destroy();
	logicalDevice = nullptr;

	physicalDevice = nullptr;

	SDL_Vulkan_DestroySurface(instance, surface, nullptr);
	surface = nullptr;

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
