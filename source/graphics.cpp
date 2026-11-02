#include "graphics.h"

#include <print>
#include <iostream>
#include <expected>
#include <algorithm>

#include <vulkan/vulkan.hpp>
#include <SDL3/SDL_vulkan.h>
#include <glm/glm.hpp>

#include "window.h"

vk::Instance g_instance;
vk::SurfaceKHR g_surface;
vk::PhysicalDevice g_physicalDevice;
vk::Device g_logicalDevice;
vk::Queue g_graphicsQueue;
vk::SwapchainKHR g_swapchain;
std::vector<vk::Image> g_images;
std::vector<vk::ImageView> g_imageViews;
vk::DebugUtilsMessengerEXT g_debugMessenger;

static bool createVulkanInstance();
static void setupDebugMessenger();
static bool createSurface();
static bool loadPhysicalDevice();
static bool createLogicalDevice();
static bool createSwapchain();

static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
	vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
	vk::DebugUtilsMessageTypeFlagsEXT type,
	const vk::DebugUtilsMessengerCallbackDataEXT* callbackData,
	void* userData);
static bool isDeviceSuitable(const vk::PhysicalDevice& device);

bool CityExplorer::initVulkan() {
	VULKAN_HPP_DEFAULT_DISPATCHER.init();

	if (!createVulkanInstance())
		goto cleanup;

	VULKAN_HPP_DEFAULT_DISPATCHER.init(g_instance);

	setupDebugMessenger();

	if (!createSurface())
		goto cleanup_debugMessenger;

	if (!loadPhysicalDevice())
		goto cleanup_surface;


	if (!createLogicalDevice())
		goto cleanup_physicalDevice;

	VULKAN_HPP_DEFAULT_DISPATCHER.init(g_logicalDevice);

	if (!createSwapchain())
		goto cleanup_logicalDevice;

	return true;

cleanup_swapChain:
	for (vk::ImageView& imageView : g_imageViews)
		g_logicalDevice.destroyImageView(imageView);

	g_imageViews.clear();
	g_imageViews.shrink_to_fit();

	g_images.clear();
	g_images.shrink_to_fit();

	g_logicalDevice.destroySwapchainKHR(g_swapchain);
	g_swapchain = nullptr;

cleanup_logicalDevice:
	g_graphicsQueue = nullptr;

	g_logicalDevice.destroy();
	g_logicalDevice = nullptr;

cleanup_physicalDevice:
	g_physicalDevice = nullptr;

cleanup_surface:
	SDL_Vulkan_DestroySurface(g_instance, g_surface, nullptr);
	g_surface = nullptr;

cleanup_debugMessenger:
	if (g_debugMessenger)
		g_instance.destroyDebugUtilsMessengerEXT(g_debugMessenger);
	g_debugMessenger = nullptr;

cleanup_instance:
	g_instance.destroy();
	g_instance = nullptr;

cleanup:
	return false;
}

void CityExplorer::shutdownVulkan() {
	for (vk::ImageView& imageView : g_imageViews)
		g_logicalDevice.destroyImageView(imageView);

	g_imageViews.clear();
	g_imageViews.shrink_to_fit();

	g_images.clear();
	g_images.shrink_to_fit();

	g_logicalDevice.destroySwapchainKHR(g_swapchain);
	g_swapchain = nullptr;

	g_graphicsQueue = nullptr;

	g_logicalDevice.destroy();
	g_logicalDevice = nullptr;

	g_physicalDevice = nullptr;

	SDL_Vulkan_DestroySurface(g_instance, g_surface, nullptr);
	g_surface = nullptr;

	if (g_debugMessenger)
		g_instance.destroyDebugUtilsMessengerEXT(g_debugMessenger);
	g_debugMessenger = nullptr;

	g_instance.destroy();
	g_instance = nullptr;
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
		g_instance = *_instance;
	else {
		std::println(std::cerr, "Could not create vulkan instance {}", vk::to_string(_instance.error()));
		return false;
	}
	return true;
}

static void setupDebugMessenger() {
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

	std::expected<vk::DebugUtilsMessengerEXT, vk::Result> _debugMessenger = g_instance.createDebugUtilsMessengerEXT(debugMessengerCreateInfo);
	if (_debugMessenger)
		g_debugMessenger = *_debugMessenger;
	else
		std::println(std::cerr, "Got error {} whilst creating debug messenger, continuing without", vk::to_string(_debugMessenger.error()));
#endif // BUILD_NRELEASE
}

static bool createSurface() {
	VkSurfaceKHR _surface;
	if (!SDL_Vulkan_CreateSurface(CityExplorer::g_window, g_instance, nullptr, &_surface)) {
		std::println(std::cerr, "SDL could not create a vulkan surface {}", SDL_GetError());
		return false;
	}

	g_surface = _surface;
	return true;
}

static bool loadPhysicalDevice() {
	std::expected<std::vector<vk::PhysicalDevice>, vk::Result> physicalDevices = g_instance.enumeratePhysicalDevices();
	if (!physicalDevices || physicalDevices->empty()) {
		std::println(std::cerr, "Failed to find GPU with Vulkan support");
		return false;
	}

	for (const vk::PhysicalDevice& pd : *physicalDevices) {
		if (g_physicalDevice && g_physicalDevice.getProperties2().properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
			break;

		// if a device has already been chosen,
		// don't evaluate the other ones if it is not a dedicated gpu
		if (g_physicalDevice)
			continue;

		if (!isDeviceSuitable(pd))
			continue;

		g_physicalDevice = pd;
	}

	if (!g_physicalDevice) {
		std::println(std::cerr, "Could not find any suitable device");
		return false;
	}

#ifdef BUILD_NRELEASE
	std::println("Found physical device {} of type {}",
		g_physicalDevice.getProperties2().properties.deviceName.data(),
		vk::to_string(g_physicalDevice.getProperties2().properties.deviceType));
#endif

	return true;
}

static bool createLogicalDevice() {
	float queuePriority = 0.5f;
	vk::DeviceQueueCreateInfo deviceQueueCreateInfo{
		.queueCount = 1,
		.pQueuePriorities = &queuePriority,
	};
	{
		std::vector<vk::QueueFamilyProperties2> queueFamilyProperties = g_physicalDevice.getQueueFamilyProperties2();
		for (size_t i = 0; i < queueFamilyProperties.size(); i++) {
			if ((queueFamilyProperties[i].queueFamilyProperties.queueFlags & vk::QueueFlagBits::eGraphics) != vk::QueueFlagBits::eGraphics)
				continue;

			if (!g_physicalDevice.getSurfaceSupportKHR(i, g_surface))
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
		{.shaderDrawParameters = true, },
		{.dynamicRendering = true, },
		{.extendedDynamicState = true, },
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

	std::expected<vk::Device, vk::Result> _logicalDevice = g_physicalDevice.createDevice(deviceCreateInfo);
	if (_logicalDevice)
		g_logicalDevice = *_logicalDevice;
	else {
		std::println(std::cerr, "Got error {} whilst creating a logical device", vk::to_string(_logicalDevice.error()));
		return false;
	}

	g_graphicsQueue = g_logicalDevice.getQueue(deviceQueueCreateInfo.queueFamilyIndex, 0);

	return true;
}

static bool createSwapchain() {
	std::expected<vk::SurfaceCapabilitiesKHR, vk::Result> surfaceCapabilities = g_physicalDevice.getSurfaceCapabilitiesKHR(g_surface);
	if (!surfaceCapabilities) {
		std::println(std::cerr, "Could not load surface capabilities {}", vk::to_string(surfaceCapabilities.error()));
		return false;
	}

	vk::Extent2D extend;

	if (surfaceCapabilities->currentExtent.width == UINT32_MAX)
		extend = surfaceCapabilities->currentExtent;
	else {
		bool gotSize = SDL_GetWindowSizeInPixels(CityExplorer::g_window, (int*) &extend.width, (int*) &extend.height);

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

	std::expected<std::vector<vk::SurfaceFormatKHR>, vk::Result> surfaceFormats = g_physicalDevice.getSurfaceFormatsKHR(g_surface);
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

	std::expected<std::vector<vk::PresentModeKHR>, vk::Result> presentModes = g_physicalDevice.getSurfacePresentModesKHR(g_surface);
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
		.surface = g_surface,
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

	std::expected<vk::SwapchainKHR, vk::Result> _swapchain = g_logicalDevice.createSwapchainKHR(swapchainCreateInfo);
	if (!_swapchain) {
		std::println(std::cerr, "Got error {} whilst creating the swapchain", vk::to_string(_swapchain.error()));
		return false;
	}
	g_swapchain = *_swapchain;

	std::expected<std::vector<vk::Image>, vk::Result> _images = g_logicalDevice.getSwapchainImagesKHR(g_swapchain);
	if (!_images) {
		std::println(std::cerr, "Got error {} whilst getting the swapchain images", vk::to_string(_images.error()));
		g_logicalDevice.destroySwapchainKHR(g_swapchain);
		g_swapchain = nullptr;
		return false;
	}
	g_images = *_images;

	g_imageViews.reserve(g_images.size());

	vk::ImageViewCreateInfo imageViewCreateInfo{
		.viewType = vk::ImageViewType::e2D,
		.format = surfaceFormat.format,
		.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 },
	};

	for (size_t i = 0; i < g_images.size(); i++) {
		imageViewCreateInfo.setImage(g_images[i]);
		std::expected<vk::ImageView, vk::Result> imageView = g_logicalDevice.createImageView(imageViewCreateInfo);
		if (!imageView) {
			std::println(std::cerr, "Got error {} whilst creating image view {}", vk::to_string(imageView.error()), i);
			for (vk::ImageView& imageView : g_imageViews)
				g_logicalDevice.destroyImageView(imageView);
			g_images.clear();
			g_images.shrink_to_fit();
			return false;
		}
		g_imageViews.emplace_back(*imageView);
	}

	return true;
}

#ifdef BUILD_NRELEASE
static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
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

static bool isDeviceSuitable(const vk::PhysicalDevice& device) {
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

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
