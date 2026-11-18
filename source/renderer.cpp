#include "renderer.h"

#include <print>
#include <iostream>
#include <expected>
#include <algorithm>
#include <filesystem>
#include <fstream>

#include <vector>
#include <vulkan/vulkan.hpp>
#include <SDL3/SDL_vulkan.h>
#include <glm/glm.hpp>

#include "platform.h"

static vk::Instance l_instance;
static vk::DebugUtilsMessengerEXT l_debugMessenger;
static vk::SurfaceKHR l_surface;
static vk::PhysicalDevice l_physicalDevice;
static vk::Device l_logicalDevice;
static vk::Queue l_graphicsQueue;
static vk::Extent2D l_swapchainExtent;
static vk::SurfaceFormatKHR l_surfaceFormat;
static vk::SwapchainKHR l_swapchain;
static std::vector<vk::Image> l_images;
static std::vector<vk::ImageView> l_imageViews;
static vk::ShaderModule l_shaderModule;
static vk::PipelineLayout l_pipelineLayout;
static vk::Pipeline l_graphicsPipeline;

static bool createVulkanInstance();
static void setupDebugMessenger();
static bool createSurface();
static bool loadPhysicalDevice();
static bool createLogicalDevice();
static bool createSwapchain();
static bool createGraphicsPipeline();

static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
	vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
	vk::DebugUtilsMessageTypeFlagsEXT type,
	const vk::DebugUtilsMessengerCallbackDataEXT* callbackData,
	void* userData);
static bool isDeviceSuitable(const vk::PhysicalDevice& device);
static vk::Extent2D getExtent(const vk::SurfaceCapabilitiesKHR& surfaceCapabilities);
static uint32_t getImageCount(const vk::SurfaceCapabilitiesKHR& surfaceCapabilities);
static vk::SurfaceFormatKHR getSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& surfaceFormats);
static vk::PresentModeKHR getPresentMode(const std::vector<vk::PresentModeKHR>& presentModes);
static bool createImages();
static std::expected<vk::ShaderModule, vk::Result> createShaderModule(std::filesystem::path path);

bool CityExplorer::initVulkan() {
	if (l_instance) {
#ifdef BUILD_NDIST
		std::println("Tried to initialize Vulkan which was already initialized, did you forget to destroy first?");
#endif
		// return true as Vulkan was already successfully initialized
		return true;
	}

	VULKAN_HPP_DEFAULT_DISPATCHER.init();

	if (!createVulkanInstance())
		goto cleanup;

	VULKAN_HPP_DEFAULT_DISPATCHER.init(l_instance);

	setupDebugMessenger();

	if (!createSurface())
		goto cleanup_debugMessenger;

	if (!loadPhysicalDevice())
		goto cleanup_surface;

	if (!createLogicalDevice())
		goto cleanup_physicalDevice;

	VULKAN_HPP_DEFAULT_DISPATCHER.init(l_logicalDevice);

	if (!createSwapchain())
		goto cleanup_logicalDevice;

	if (!createGraphicsPipeline())
		goto cleanup_swapchain;

	return true;

cleanup_graphicsPipeline:
	l_logicalDevice.destroyPipeline(l_graphicsPipeline);
	l_logicalDevice.destroyPipelineLayout(l_pipelineLayout);
	l_logicalDevice.destroyShaderModule(l_shaderModule);
	l_logicalDevice = nullptr;

cleanup_swapchain:
	for (vk::ImageView& imageView : l_imageViews)
		l_logicalDevice.destroyImageView(imageView);
	l_imageViews.clear();
	l_imageViews.shrink_to_fit();

	l_images.clear();
	l_images.shrink_to_fit();

	l_logicalDevice.destroySwapchainKHR(l_swapchain);
	l_swapchain = nullptr;

cleanup_logicalDevice:
	l_graphicsQueue = nullptr;

	l_logicalDevice.destroy();
	l_logicalDevice = nullptr;

cleanup_physicalDevice:
	l_physicalDevice = nullptr;

cleanup_surface:
	SDL_Vulkan_DestroySurface(l_instance, l_surface, nullptr);
	l_surface = nullptr;

cleanup_debugMessenger:
	if (l_debugMessenger)
		l_instance.destroyDebugUtilsMessengerEXT(l_debugMessenger);
	l_debugMessenger = nullptr;

cleanup_instance:
	l_instance.destroy();
	l_instance = nullptr;

cleanup:
	return false;
}

void CityExplorer::shutdownVulkan() {
	l_logicalDevice.destroyPipeline(l_graphicsPipeline);
	l_logicalDevice.destroyPipelineLayout(l_pipelineLayout);
	l_logicalDevice.destroyShaderModule(l_shaderModule);
	l_shaderModule = nullptr;

	for (vk::ImageView& imageView : l_imageViews)
		l_logicalDevice.destroyImageView(imageView);
	l_imageViews.clear();
	l_imageViews.shrink_to_fit();

	l_images.clear();
	l_images.shrink_to_fit();

	l_logicalDevice.destroySwapchainKHR(l_swapchain);
	l_swapchain = nullptr;

	l_graphicsQueue = nullptr;

	l_logicalDevice.destroy();
	l_logicalDevice = nullptr;

	l_physicalDevice = nullptr;

	SDL_Vulkan_DestroySurface(l_instance, l_surface, nullptr);
	l_surface = nullptr;

	if (l_debugMessenger)
		l_instance.destroyDebugUtilsMessengerEXT(l_debugMessenger);
	l_debugMessenger = nullptr;

	l_instance.destroy();
	l_instance = nullptr;
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
#ifdef BUILD_NDIST
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
#ifdef BUILD_NDIST
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

	std::expected<vk::Instance, vk::Result> instance = vk::createInstance(createInfo)
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

	if (instance)
		l_instance = *instance;
	else {
		std::println(std::cerr, "Could not create vulkan instance {}", vk::to_string(_instance.error()));
		return false;
	}
	return true;
}

static void setupDebugMessenger() {
#ifdef BUILD_NDIST
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

	std::expected<vk::DebugUtilsMessengerEXT, vk::Result> debugMessenger = l_instance.createDebugUtilsMessengerEXT(debugMessengerCreateInfo);
	if (debugMessenger)
		l_debugMessenger = *debugMessenger;
	else
		std::println(std::cerr, "Got error {} whilst creating debug messenger, continuing without", vk::to_string(_debugMessenger.error()));
#endif // BUILD_NDIST
}

static bool createSurface() {
	VkSurfaceKHR surface;
	if (!SDL_Vulkan_CreateSurface(CityExplorer::g_window, l_instance, nullptr, &surface)) {
		std::println(std::cerr, "SDL could not create a vulkan surface {}", SDL_GetError());
		return false;
	}

	l_surface = surface;
	return true;
}

static bool loadPhysicalDevice() {
	std::expected<std::vector<vk::PhysicalDevice>, vk::Result> physicalDevices = l_instance.enumeratePhysicalDevices();
	if (!physicalDevices || physicalDevices->empty()) {
		std::println(std::cerr, "Failed to find GPU with Vulkan support");
		return false;
	}

	for (const vk::PhysicalDevice& pd : *physicalDevices) {
		if (l_physicalDevice && l_physicalDevice.getProperties2().properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
			break;

		// if a device has already been chosen,
		// don't evaluate the other ones if it is not a dedicated gpu
		if (l_physicalDevice)
			continue;

		if (!isDeviceSuitable(pd))
			continue;

		l_physicalDevice = pd;
	}

	if (!l_physicalDevice) {
		std::println(std::cerr, "Could not find any suitable device");
		return false;
	}

#ifdef BUILD_NDIST
	std::println("Found physical device {} of type {}",
		l_physicalDevice.getProperties2().properties.deviceName.data(),
		vk::to_string(l_physicalDevice.getProperties2().properties.deviceType));
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
		std::vector<vk::QueueFamilyProperties2> queueFamilyProperties = l_physicalDevice.getQueueFamilyProperties2();
		for (size_t i = 0; i < queueFamilyProperties.size(); i++) {
			if ((queueFamilyProperties[i].queueFamilyProperties.queueFlags & vk::QueueFlagBits::eGraphics) != vk::QueueFlagBits::eGraphics)
				continue;

			if (!l_physicalDevice.getSurfaceSupportKHR(i, l_surface))
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

	std::expected<vk::Device, vk::Result> logicalDevice = l_physicalDevice.createDevice(deviceCreateInfo);
	if (logicalDevice)
		l_logicalDevice = *logicalDevice;
	else {
		std::println(std::cerr, "Got error {} whilst creating a logical device", vk::to_string(_logicalDevice.error()));
		return false;
	}

	l_graphicsQueue = l_logicalDevice.getQueue(deviceQueueCreateInfo.queueFamilyIndex, 0);

	return true;
}

static bool createSwapchain() {
	std::expected<vk::SurfaceCapabilitiesKHR, vk::Result> surfaceCapabilities = l_physicalDevice.getSurfaceCapabilitiesKHR(l_surface);
	if (!surfaceCapabilities) {
		std::println(std::cerr, "Could not load surface capabilities {}", vk::to_string(surfaceCapabilities.error()));
		return false;
	}

	std::expected<std::vector<vk::SurfaceFormatKHR>, vk::Result> surfaceFormats = l_physicalDevice.getSurfaceFormatsKHR(l_surface);
	if (!surfaceFormats || surfaceFormats->empty()) {
		std::println(std::cerr, "Could not load surface formats {}", vk::to_string(surfaceFormats.error()));
		return false;
	}

	std::expected<std::vector<vk::PresentModeKHR>, vk::Result> presentModes = l_physicalDevice.getSurfacePresentModesKHR(l_surface);
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

	l_swapchainExtent = getExtent(*surfaceCapabilities);
	uint32_t imageCount = getImageCount(*surfaceCapabilities);
	l_surfaceFormat = getSurfaceFormat(*surfaceFormats);
	vk::PresentModeKHR presentMode = getPresentMode(*presentModes);

#ifdef BUILD_DEBUG
	std::println("Using surface extend of {{{}, {}}}", l_swapchainExtent.width, l_swapchainExtent.height);
	std::println("Using {} images in swapChain", imageCount);
	std::println("Using surface format {}", vk::to_string(l_surfaceFormat.format));
	std::println("Using surface color space {}", vk::to_string(l_surfaceFormat.colorSpace));
	std::println("Using present mode {}", vk::to_string(presentMode));
#endif

	vk::SwapchainCreateInfoKHR swapchainCreateInfo{
		.surface = l_surface,
		.minImageCount = imageCount,
		.imageFormat = l_surfaceFormat.format,
		.imageColorSpace = l_surfaceFormat.colorSpace,
		.imageExtent = l_swapchainExtent,
		.imageArrayLayers = 1,
		.imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
		.imageSharingMode = vk::SharingMode::eExclusive,
		.preTransform = surfaceCapabilities->currentTransform,
		.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
		.presentMode = presentMode,
		.clipped = true,
	};

	std::expected<vk::SwapchainKHR, vk::Result> swapchain = l_logicalDevice.createSwapchainKHR(swapchainCreateInfo);
	if (!swapchain) {
		std::println(std::cerr, "Got error {} whilst creating the swapchain", vk::to_string(_swapchain.error()));
		return false;
	}
	l_swapchain = *swapchain;

	if (!createImages()) {
		std::println(std::cerr, "Could not create images");
		l_logicalDevice.destroySwapchainKHR(l_swapchain);
		l_swapchain = nullptr;
		return false;
	}

	return true;
}

static bool createGraphicsPipeline() {
	std::expected<vk::ShaderModule, vk::Result> shaderModule = createShaderModule(std::filesystem::path("assets") / "shaders" / "shader.spv");
	if (!shaderModule) {
		std::println(std::cerr, "Got error {} whilst creating shader module", vk::to_string(shaderModule.error()));
		return false;
	}
	l_shaderModule = *shaderModule;

	vk::PipelineShaderStageCreateInfo vertShaderStageInfo{ .stage = vk::ShaderStageFlagBits::eVertex, .module = l_shaderModule, .pName = "vertMain", };
	vk::PipelineShaderStageCreateInfo fragShaderStageInfo{ .stage = vk::ShaderStageFlagBits::eFragment, .module = l_shaderModule, .pName = "fragMain", };

	vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo, };

	vk::PipelineVertexInputStateCreateInfo vertexInputInfo = {};
	vk::PipelineInputAssemblyStateCreateInfo inputAssembly{ .topology = vk::PrimitiveTopology::eTriangleList, };

	std::vector<vk::DynamicState> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor, };
	vk::PipelineDynamicStateCreateInfo dynamicState{ .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()), .pDynamicStates = dynamicStates.data(), };
	vk::Viewport viewport = { 0.0f, 0.0f, static_cast<float>(l_swapchainExtent.width), static_cast<float>(l_swapchainExtent.height), 0.0f, 1.0f, };
	vk::Rect2D scissor = { { 0, 0 }, l_swapchainExtent };
	vk::PipelineViewportStateCreateInfo viewportState = { .viewportCount = 1, .pViewports = &viewport, .scissorCount = 1, .pScissors = &scissor, };

	vk::PipelineRasterizationStateCreateInfo rasterizer = {
		.depthClampEnable = vk::False,
		.rasterizerDiscardEnable = vk::False,
		.polygonMode = vk::PolygonMode::eFill,
		.cullMode = vk::CullModeFlagBits::eBack,
		.frontFace = vk::FrontFace::eClockwise,
		.depthBiasEnable = vk::False,
		.lineWidth = 1.0f,
	};

	vk::PipelineMultisampleStateCreateInfo multisampling = { .rasterizationSamples = vk::SampleCountFlagBits::e1, .sampleShadingEnable = vk::False, };

	vk::PipelineColorBlendAttachmentState colorBlendAttachment = {
		.blendEnable = vk::True,
		.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
		.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
		.colorBlendOp = vk::BlendOp::eAdd,
		.srcAlphaBlendFactor = vk::BlendFactor::eOne,
		.dstAlphaBlendFactor = vk::BlendFactor::eZero,
		.alphaBlendOp = vk::BlendOp::eAdd,
		.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
	};
	vk::PipelineColorBlendStateCreateInfo colorBlending = {
		.logicOpEnable = vk::False,
		.logicOp = vk::LogicOp::eCopy,
		.attachmentCount = 1,
		.pAttachments = &colorBlendAttachment,
	};

	vk::PipelineLayoutCreateInfo pipelineLayoutInfo{ .setLayoutCount = 0, .pushConstantRangeCount = 0, };

	std::expected<vk::PipelineLayout, vk::Result> pipelineLayout = l_logicalDevice.createPipelineLayout(pipelineLayoutInfo);
	if (pipelineLayout)
		l_pipelineLayout = *pipelineLayout;
	else {
		std::println(std::cerr, "Gotten error {} during pipeline creation", vk::to_string(pipelineLayout.error()));
		return false;
	}

	vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain = {
		{
			.stageCount = 2,
			.pStages = shaderStages,
			.pVertexInputState = &vertexInputInfo,
			.pInputAssemblyState = &inputAssembly,
			.pViewportState = &viewportState,
			.pRasterizationState = &rasterizer,
			.pMultisampleState = &multisampling,
			.pColorBlendState = &colorBlending,
			.pDynamicState = &dynamicState,
			.layout = l_pipelineLayout,
			.renderPass = nullptr,
		}, {
			.colorAttachmentCount = 1,
			.pColorAttachmentFormats = &l_surfaceFormat.format,
		}
	};

	vk::ResultValue<vk::Pipeline> pipeline = l_logicalDevice.createGraphicsPipeline(nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
	if (!pipeline.has_value()) {
		std::println(std::cerr, "got error {} whilst creating graphics pipeline", vk::to_string(pipeline.result));
		l_logicalDevice.destroyPipelineLayout(l_pipelineLayout);
		l_pipelineLayout = nullptr;
		return false;
	}
	l_graphicsPipeline = pipeline.value;

	return true;
}

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

static bool isDeviceSuitable(const vk::PhysicalDevice& device) {
	const std::vector<const char*> requiredExtensions{
		vk::KHRSwapchainExtensionName,
	};

#ifdef BUILD_NDIST
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

static vk::Extent2D getExtent(const vk::SurfaceCapabilitiesKHR& surfaceCapabilities) {
	vk::Extent2D extent;

	if (surfaceCapabilities.currentExtent.width != UINT32_MAX)
		return surfaceCapabilities.currentExtent;

	bool gotSize = SDL_GetWindowSizeInPixels(CityExplorer::g_window, (int*) &extent.width, (int*) &extent.height);

	if (gotSize) {
		extent.width = glm::clamp(extent.width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
		extent.height = glm::clamp(extent.height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);
		return extent;
	}

#ifdef BUILD_NDIST
	std::println(std::cerr, "Could not get SDL frame size, got error {}, using minimal size", SDL_GetError());
#endif
	return surfaceCapabilities.minImageExtent;
}

static uint32_t getImageCount(const vk::SurfaceCapabilitiesKHR& surfaceCapabilities) {
	uint32_t imageCount = 3;
	if (imageCount < surfaceCapabilities.minImageCount)
		imageCount = surfaceCapabilities.minImageCount;

	if (surfaceCapabilities.maxImageCount != 0 && surfaceCapabilities.maxImageCount < imageCount)
		imageCount = surfaceCapabilities.maxImageCount;

	return imageCount;
}

static vk::SurfaceFormatKHR getSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& surfaceFormats) {
	for (const vk::SurfaceFormatKHR& format : surfaceFormats) {
		if (format.format != vk::Format::eR8G8B8A8Srgb)
			continue;

		if (format.colorSpace != vk::ColorSpaceKHR::eSrgbNonlinear)
			continue;

		return format;
	}
	return surfaceFormats[0];
}

static vk::PresentModeKHR getPresentMode(const std::vector<vk::PresentModeKHR>& presentModes) {
	for (const vk::PresentModeKHR& mode : presentModes) {
		if (mode != vk::PresentModeKHR::eMailbox)
			continue;

		return mode;
	}
	return vk::PresentModeKHR::eFifo;
}

static bool createImages() {
	vk::ImageViewCreateInfo imageViewCreateInfo{
		.viewType = vk::ImageViewType::e2D,
		.format = l_surfaceFormat.format,
		.subresourceRange = {
			.aspectMask = vk::ImageAspectFlagBits::eColor,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer= 0,
			.layerCount= 1,
		},
	};

	std::expected<std::vector<vk::Image>, vk::Result> images = l_logicalDevice.getSwapchainImagesKHR(l_swapchain);
	if (!images) {
		std::println(std::cerr, "Got error {} whilst getting the swapchain images", vk::to_string(_images.error()));
		goto cleanup;
	}
	l_images = *images;

	l_imageViews.reserve(l_images.size());
	for (size_t i = 0; i < l_images.size(); i++) {
		imageViewCreateInfo.setImage(l_images[i]);
		std::expected<vk::ImageView, vk::Result> imageView = l_logicalDevice.createImageView(imageViewCreateInfo);
		if (!imageView) {
			std::println(std::cerr, "Got error {} whilst creating image view {}", vk::to_string(imageView.error()), i);
			goto cleanup_imageViews;
		}
		l_imageViews.emplace_back(*imageView);
	}

	return true;

cleanup_imageViews:
	for (vk::ImageView& imageView : l_imageViews)
		l_logicalDevice.destroyImageView(imageView);
	l_imageViews.clear();
	l_imageViews.shrink_to_fit();

cleanup_images:
	l_images.clear();
	l_images.shrink_to_fit();

cleanup:
	return false;
}

static std::expected<vk::ShaderModule, vk::Result> createShaderModule(std::filesystem::path path) {
	std::vector<char> code;
	{
		std::ifstream file;

		file.open(std::filesystem::path("assets") / "shaders" / "shader.spv", std::ios::ate | std::ios::binary);
		if (file) {
			code = std::vector<char>(file.tellg());
			file.seekg(0).read(code.data(), static_cast<std::streamsize>(code.size()));
		}
		file.close();
	}
	vk::ShaderModuleCreateInfo createInfo{ .codeSize = code.size(), .pCode = reinterpret_cast<const uint32_t*>(code.data()) };
	return l_logicalDevice.createShaderModule(createInfo);
}

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
