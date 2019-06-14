#include "Renderer.h"
#include <stdexcept>
#include <iostream>
#include <algorithm>
#include <set>

#define GLFW_FORCE_RADIANS
#define GLFW_FORCE_DEPTH_ZERO_TO_ONE

#include "glm.hpp"

const DynamicArray<const char*> Renderer::m_validationLayers = 
{
	"VK_LAYER_KHRONOS_validation"
};

VkResult Renderer::safeCallResult = VK_SUCCESS;

const DynamicArray<const char*> Renderer::m_deviceExtensions =
{
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifndef RENDERER_DEBUG

const bool Renderer::m_enableValidationLayers = false;

#else

const bool Renderer::m_enableValidationLayers = true;

#endif

Renderer::Renderer(GLFWwindow* window)
{
	m_window = window;
	m_extensions = nullptr;
	m_extensionCount = 0;

	m_windowWidth = WINDOW_WIDTH;
	m_windowHeight = WINDOW_HEIGHT;

	m_instance = VK_NULL_HANDLE;
	m_physDevice = VK_NULL_HANDLE;

	m_graphicsQueueFamilyIndex = -1;
	m_presentQueueFamilyIndex = -1;
	m_computeQueueFamilyIndex = -1;

	CheckValidationLayerSupport();
	CreateVKInstance();
	SetupDebugMessenger();
	CreateWindowSurface();
	GetPhysicalDevice();
	CreateLogicalDevice();
	CreateSwapChain();

	// Get queue handles...
	vkGetDeviceQueue(m_logicDevice, m_presentQueueFamilyIndex, 0, &m_presentQueue);
	vkGetDeviceQueue(m_logicDevice, m_graphicsQueueFamilyIndex, 0, &m_graphicsQueue);
	vkGetDeviceQueue(m_logicDevice, m_computeQueueFamilyIndex, 0, &m_computeQueue);
}

Renderer::~Renderer()
{
	delete[] m_extensions;

	// Destroy debug messenger.
	DestroyDebugUtilsMessengerEXT(m_instance, nullptr, &m_messenger);

	// Destroy image views.
	for (int i = 0; i < m_swapChainImageViews.Count(); ++i)
		vkDestroyImageView(m_logicDevice, m_swapChainImageViews[i], nullptr);

	// Destroy swap chain.
	vkDestroySwapchainKHR(m_logicDevice, m_swapChain, nullptr);

	// Destroy logical device.
	vkDestroyDevice(m_logicDevice, nullptr);

	// Destroy window surface.
	vkDestroySurfaceKHR(m_instance, m_windowSurface, nullptr);

	// Destroy Vulkan instance.
	vkDestroyInstance(m_instance, nullptr);
}

VKAPI_ATTR VkBool32 VKAPI_CALL Renderer::ErrorCallback
(
	VkDebugUtilsMessageSeverityFlagBitsEXT severity,
	VkDebugUtilsMessageTypeFlagsEXT type,
	const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
	void* userData
) 
{
	if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		std::cout << "Vulkan Validation Error: " << callbackData->pMessage << std::endl;
	else if(severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		std::cout << "Vulkan Validation Warning: " << callbackData->pMessage << std::endl;
	else
		std::cout << "Vulkan Validation Info: " << callbackData->pMessage << std::endl;

	return VK_FALSE;
}

void Renderer::CreateVKInstance() 
{
	VkResult result = {};

	// ----------------------------------------------------------------------------------------------------------
	// Extension count.

	result = vkEnumerateInstanceExtensionProperties(nullptr, &m_extensionCount, nullptr);

	m_extensions = new VkExtensionProperties[m_extensionCount];

	if (result == VK_INCOMPLETE)
		result = VK_SUCCESS;

	// ----------------------------------------------------------------------------------------------------------
	// Application info to give to Vulkan.
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "VulkanRenderer";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	// ----------------------------------------------------------------------------------------------------------
	// Instance info.

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

#ifdef RENDERER_DEBUG
	// Include validation layers.
	createInfo.ppEnabledLayerNames = m_validationLayers.Data();
	createInfo.enabledLayerCount = m_validationLayers.Count();
#else
	createInfo.enabledLayerCount = 0;

#endif

	unsigned int glfwExtensionCount = 0;
	const char** glfwExtensions;

	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

#ifdef RENDERER_DEBUG
	// Expand array by 1...
	const char** extensions = new const char*[glfwExtensionCount + 1];
	memcpy_s(extensions, sizeof(const char*) * (glfwExtensionCount + 1), glfwExtensions, sizeof(const char*) * glfwExtensionCount);

	glfwExtensions = extensions;

	// Add debug extension.
	glfwExtensions[glfwExtensionCount] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

	++glfwExtensionCount;
#endif

	createInfo.enabledExtensionCount = glfwExtensionCount;
	createInfo.ppEnabledExtensionNames = glfwExtensions;

	// ----------------------------------------------------------------------------------------------------------
	// Create instance.

	RENDERER_SAFECALL(vkCreateInstance(&createInfo, 0, &m_instance), "Renderer Error: Failed to create Vulkan instance!");

#ifdef RENDERER_DEBUG
	delete[] extensions;
#endif

	// ----------------------------------------------------------------------------------------------------------
	// Extensions...

	result = vkEnumerateInstanceExtensionProperties(nullptr, &m_extensionCount, m_extensions);

	std::cout << "Renderer Info: Available extensions are: \n" << std::endl;

	for (unsigned int i = 0; i < m_extensionCount; ++i)
		std::cout << m_extensions[i].extensionName << std::endl;
}

void Renderer::CheckValidationLayerSupport() 
{
	if (!m_enableValidationLayers)
		return;

	// Get available validation layers...
	unsigned int layerCount = 0;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	DynamicArray<VkLayerProperties> availableLayers(layerCount, 1);
	for (int i = 0; i < availableLayers.GetSize(); ++i)
		availableLayers.Push(VkLayerProperties());

	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.Data());

	// Compare against required layers...
	int successCount = 0;
	for(int i = 0; i < m_validationLayers.Count(); ++i) 
	{
		for (int j = 0; j < availableLayers.Count(); ++j)
		{
			if (strcmp(m_validationLayers[i], availableLayers[j].layerName) == 0)
			{
				++successCount;
				break;
			}
		}
	}

	if (successCount == m_validationLayers.Count())
		std::cout << "Renderer Info: All necessary validation layers found." << std::endl;
	else
		std::cout << "Renderer Warning: Not all necessary validation layers were found!" << std::endl;
}

bool Renderer::CheckDeviceExtensionSupport(VkPhysicalDevice device) 
{
	unsigned int extensionCount = 0;
	RENDERER_SAFECALL(vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr), "Renderer Error: Failed to obtain Vulkan extension count");

	DynamicArray<VkExtensionProperties> extensions(extensionCount, 1);
	for (unsigned int i = 0; i < extensionCount; ++i)
		extensions.Push(VkExtensionProperties());

	RENDERER_SAFECALL(vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.Data()), "Renderer Error: Failed to obtain Vulkan extension properties");

	int requiredExtensionCount = 0;
	for(int i = 0; i < m_deviceExtensions.Count(); ++i) 
	{
		for(int j = 0; j < extensions.Count(); ++j) 
		{
			if (strcmp(m_deviceExtensions[i], extensions[j].extensionName) == 0) 
			{
				++requiredExtensionCount;
				break;
			}
		}
	}

	return requiredExtensionCount == m_deviceExtensions.Count();
}

Renderer::SwapChainDetails* Renderer::GetSwapChainSupportDetails(VkPhysicalDevice device) 
{
	SwapChainDetails* details = new SwapChainDetails;

	// Get capabilities.
	RENDERER_SAFECALL(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_windowSurface, &details->m_capabilities), "Renderer Error: Failed to obtain window surface capabilities.");

	// Get formats...
	unsigned int formatCount = 0;
	RENDERER_SAFECALL(vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_windowSurface, &formatCount, nullptr), "Renderer Error: Failed to obtain window surface format count.");

	if(formatCount > 0) 
	{
		details->m_formats.SetSize(formatCount);
		for (unsigned int i = 0; i < formatCount; ++i)
			details->m_formats.Push(VkSurfaceFormatKHR());

		RENDERER_SAFECALL(vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_windowSurface, &formatCount, details->m_formats.Data()), "Renderer Error: Failed to obtain window surface formats.");
	}

	// Get present modes...
	unsigned int presentModeCount = 0;
	RENDERER_SAFECALL(vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_windowSurface, &presentModeCount, nullptr), "Renderer Error: Failed to obtain window surface present mode count.");

	if (presentModeCount > 0)
	{
		details->m_presentModes.SetSize(presentModeCount);
		for (unsigned int i = 0; i < presentModeCount; ++i)
			details->m_presentModes.Push(VkPresentModeKHR());

		RENDERER_SAFECALL(vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_windowSurface, &presentModeCount, details->m_presentModes.Data()), "Renderer Error: Failed to obtain window surface present modes.");
	}

	return details;
}

VkResult Renderer::CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* createInfo, const VkAllocationCallbacks* allocator, VkDebugUtilsMessengerEXT* messenger) 
{
	if (!m_enableValidationLayers)
		return VK_ERROR_NOT_PERMITTED_EXT;

	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

	if (func)
	{
		return func(instance, createInfo, allocator, messenger);
	}
	else
		return VK_ERROR_EXTENSION_NOT_PRESENT;
}

VkResult Renderer::DestroyDebugUtilsMessengerEXT(VkInstance instance, const VkAllocationCallbacks* allocator, VkDebugUtilsMessengerEXT* messenger)
{
	if (!m_enableValidationLayers)
		return VK_ERROR_NOT_PERMITTED_EXT;

	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

	if (func)
	{
		func(instance, *messenger, allocator);

		return VK_SUCCESS;
	}
	else
		return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void Renderer::SetupDebugMessenger() 
{
	if (!m_enableValidationLayers)
		return;

	VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = &ErrorCallback;
	createInfo.pUserData = nullptr;

	RENDERER_SAFECALL(CreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_messenger), "Renderer Error: Failed to create debug messenger!");
}

int Renderer::DeviceSuitable(VkPhysicalDevice device) 
{
	if (device == VK_NULL_HANDLE)
		throw std::runtime_error("Renderer Error: Cannot check suitability for a null device!");

	VkPhysicalDeviceProperties properties = {};
	vkGetPhysicalDeviceProperties(device, &properties);

	VkPhysicalDeviceFeatures features = {};
	vkGetPhysicalDeviceFeatures(device, &features);

	bool extensionsSupported = CheckDeviceExtensionSupport(device);

	if (!extensionsSupported)
		return 0;

	SwapChainDetails* swapChainDetails = GetSwapChainSupportDetails(device);
	bool suitableSwapChain = swapChainDetails->m_formats.Count() > 0 && swapChainDetails->m_presentModes.Count() > 0;

	delete swapChainDetails;

	if (!suitableSwapChain)
		return 0;

	// A device is suitable if it is a GPU with geometry shader support.
	int score = properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && FindQueueFamilies(device) && features.geometryShader;

	if (score == 0)
		return score;

	score += properties.limits.maxImageDimension2D;
	score += properties.limits.maxFramebufferWidth;
	score += properties.limits.maxFramebufferHeight;
	score += properties.limits.maxColorAttachments;
	score += properties.limits.maxMemoryAllocationCount;

	return score;
}

void Renderer::GetPhysicalDevice() 
{
	unsigned int deviceCount = 0;
	RENDERER_SAFECALL(vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr), "Renderer Error: Failed to get physical device count.");

	DynamicArray<VkPhysicalDevice> devices(deviceCount, 1);
	for (unsigned int i = 0; i < deviceCount; ++i)
		devices.Push(VkPhysicalDevice());

	RENDERER_SAFECALL(vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.Data()), "Renderer Error: Failed to obtain physical device data.");

	// Check if devices are suitable and the first suitable device.
	int bestScore = 0;
	for(unsigned int i = 0; i < deviceCount; ++i) 
	{
		int suitabilityScore = DeviceSuitable(devices[i]);
		if (suitabilityScore > bestScore) 
		{
			m_physDevice = devices[i];
		}
	}

	if (m_physDevice == VK_NULL_HANDLE)
		throw std::runtime_error("Renderer Error: Failed to find suitable GPU!");
}

bool Renderer::FindQueueFamilies(VkPhysicalDevice device) 
{
	unsigned int queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	DynamicArray<VkQueueFamilyProperties> queueFamilies(queueFamilyCount, 1);
	for (unsigned int i = 0; i < queueFamilyCount; ++i)
		queueFamilies.Push(VkQueueFamilyProperties());

	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.Data());

	// Get queue indices.
	for(unsigned int i = 0; i < queueFamilyCount; ++i) 
	{
		VkBool32 hasPresentSupport = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_windowSurface, &hasPresentSupport);

		if (queueFamilies[i].queueCount > 0 && hasPresentSupport)
		{
			m_presentQueueFamilyIndex = i;
		}

		if (queueFamilies[i].queueCount > 0 && queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			m_graphicsQueueFamilyIndex = i;
		}

		if (queueFamilies[i].queueCount > 0 && queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
		{
			m_computeQueueFamilyIndex = i;
		}
	}

	return m_presentQueueFamilyIndex > -1 && m_graphicsQueueFamilyIndex > -1 && m_computeQueueFamilyIndex > -1;
}

void Renderer::CreateLogicalDevice()
{
	DynamicArray<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<int> uniqueQueueFamilies = { m_presentQueueFamilyIndex, m_graphicsQueueFamilyIndex, m_computeQueueFamilyIndex };

	float queuePriority = QUEUE_PRIORITY;

	// Create queue infos for each unique queue.
	for(int familyIndex : uniqueQueueFamilies) 
	{
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = familyIndex;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;

		queueCreateInfos.Push(queueCreateInfo);
	}

	VkPhysicalDeviceFeatures features = {};
	vkGetPhysicalDeviceFeatures(m_physDevice, &features);

	VkDeviceCreateInfo logicDeviceCreateInfo = {};
	logicDeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	logicDeviceCreateInfo.pQueueCreateInfos = queueCreateInfos.Data();
	logicDeviceCreateInfo.queueCreateInfoCount = queueCreateInfos.Count();
	logicDeviceCreateInfo.pEnabledFeatures = &features;
	logicDeviceCreateInfo.enabledExtensionCount = m_deviceExtensions.Count();
	logicDeviceCreateInfo.ppEnabledExtensionNames = m_deviceExtensions.Data();

	// Logical device validation layers
	if (m_enableValidationLayers)
	{
		logicDeviceCreateInfo.ppEnabledLayerNames = m_validationLayers.Data();
		logicDeviceCreateInfo.enabledLayerCount = m_validationLayers.Count();
	}
	else
		logicDeviceCreateInfo.enabledLayerCount = 0;

	RENDERER_SAFECALL(vkCreateDevice(m_physDevice, &logicDeviceCreateInfo, nullptr, &m_logicDevice), "Renderer Error: Failed to create logical device!");
}

void Renderer::CreateWindowSurface() 
{
	RENDERER_SAFECALL(glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_windowSurface), "Renderer Error: Failed to obtain window surface.");
}

void Renderer::CreateSwapChain() 
{
	SwapChainDetails* details = GetSwapChainSupportDetails(m_physDevice);

	VkSurfaceFormatKHR format = ChooseSwapSurfaceFormat(details->m_formats);
	VkPresentModeKHR presentMode = ChooseSwapPresentMode(details->m_presentModes);
	m_swapChainImageExtents = ChooseSwapExtent(details->m_capabilities);
	m_swapChainImageFormat = format.format;

	// Find appropriate image count.
	unsigned int imageCount = details->m_capabilities.minImageCount + 1;

	if (details->m_capabilities.maxImageCount > 0 && imageCount > details->m_capabilities.maxImageCount)
		imageCount = details->m_capabilities.maxImageCount;

	VkSwapchainCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = m_windowSurface;
	createInfo.imageFormat = format.format;
	createInfo.imageColorSpace = format.colorSpace;
	createInfo.presentMode = presentMode;
	createInfo.imageExtent = m_swapChainImageExtents;
	createInfo.minImageCount = imageCount;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.preTransform = details->m_capabilities.currentTransform;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	unsigned int queueFamilyIndices[2] = 
	{ 
		static_cast<unsigned int>(m_presentQueueFamilyIndex), 
		static_cast<unsigned int>(m_graphicsQueueFamilyIndex) 
	};

	if(m_graphicsQueueFamilyIndex != m_presentQueueFamilyIndex) 
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else 
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0;
		createInfo.pQueueFamilyIndices = nullptr;
	}

	vkCreateSwapchainKHR(m_logicDevice, &createInfo, nullptr, &m_swapChain);

	// Retrieve swap chain images.
	unsigned int swapChainImageCount = 0;
	RENDERER_SAFECALL(vkGetSwapchainImagesKHR(m_logicDevice, m_swapChain, &swapChainImageCount, nullptr), "Renderer Error: Failed to retrieve swap chain image count.");

	m_swapChainImages.SetSize(swapChainImageCount);
	for (unsigned int i = 0; i < swapChainImageCount; ++i)
		m_swapChainImages.Push(VkImage());

	RENDERER_SAFECALL(vkGetSwapchainImagesKHR(m_logicDevice, m_swapChain, &swapChainImageCount, m_swapChainImages.Data()), "Renderer Error: Failed to retieve swap chain images.");

	delete details;
}

void Renderer::CreateSwapChainImageViews() 
{
	m_swapChainImageViews.SetSize(m_swapChainImages.Count());
	for (int i = 0; i < m_swapChainImages.Count(); ++i) 
	{
		VkImageView view;
		VkImageViewCreateInfo viewCreateInfo = {};

		viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCreateInfo.image = m_swapChainImages[i];
		viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewCreateInfo.format = m_swapChainImageFormat;
		viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

		viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewCreateInfo.subresourceRange.baseMipLevel = 0;
		viewCreateInfo.subresourceRange.levelCount = 1;
		viewCreateInfo.subresourceRange.baseArrayLayer = 0;
		viewCreateInfo.subresourceRange.layerCount = 0;

		RENDERER_SAFECALL(vkCreateImageView(m_logicDevice, &viewCreateInfo, nullptr, &view), "Renderer Error: Failed to create swap chain image views.");

		m_swapChainImageViews.Push(view);
	}
}

void Renderer::CreateGraphicsPipeline() 
{

}

VkSurfaceFormatKHR Renderer::ChooseSwapSurfaceFormat(DynamicArray<VkSurfaceFormatKHR>& availableFormats) 
{
	VkSurfaceFormatKHR desiredFormat = { VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };

	if (availableFormats.Count() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED) // Can use any format.
	{
		return desiredFormat; // So use the desired format.
	}

	for (int i = 0; i < availableFormats.Count(); ++i) 
	{
		if (availableFormats[i].format == desiredFormat.format && availableFormats[i].colorSpace == desiredFormat.colorSpace) // Found the desired format.
			return desiredFormat;
	}

	// Otherwise the desired format was not found, just use the first one.
	return availableFormats[0];
}

VkPresentModeKHR Renderer::ChooseSwapPresentMode(DynamicArray<VkPresentModeKHR>& availablePresentModes) 
{
	VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR; // FIFO_KHR is guaranteed to be available.

	for (int i = 0; i < availablePresentModes.Count(); ++i) 
	{
		if (availablePresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
			return availablePresentModes[i]; // Use mailbox if it is available.
		else if (availablePresentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
			bestMode = VK_PRESENT_MODE_IMMEDIATE_KHR; // Otherwise immediate is the next best mode.
	}

	return bestMode;
}

VkExtent2D Renderer::ChooseSwapExtent(VkSurfaceCapabilitiesKHR capabilities) 
{
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		return capabilities.currentExtent; // Use provided extent since it is the only available one.

	VkExtent2D desiredExtent = { m_windowWidth, m_windowHeight };

	// Clamp to usable range.
	desiredExtent.width = glm::clamp(desiredExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
	desiredExtent.height = glm::clamp(desiredExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

	return desiredExtent;
}