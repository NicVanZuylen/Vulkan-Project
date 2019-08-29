#include "RendererHelper.h"

VkFormat RendererHelper::FindBestDepthFormat(VkPhysicalDevice physDevice, DynamicArray<VkFormat>& formats, VkImageTiling tiling, VkFormatFeatureFlags features)
{
	for (int i = 0; i < formats.Count(); ++i)
	{
		VkFormatProperties properties;

		// Get format properties.
		vkGetPhysicalDeviceFormatProperties(physDevice, formats[i], &properties);

		// Find matching format.
		if (tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & features) == features)
		{
			return formats[i];
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & features) == features)
			return formats[i];
	}

	throw std::exception("Renderer Error: Failed to find suitable format.");

	return formats[0];
}

bool RendererHelper::CheckDeviceExtensionSupport(const DynamicArray<const char*>& extensionNames, VkPhysicalDevice device)
{
	// Get amount of available device extensions.
	unsigned int extensionCount = 0;
	RENDERER_SAFECALL(vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr), "Renderer Error: Failed to obtain Vulkan extension count");

	DynamicArray<VkExtensionProperties> extensions(extensionCount, 1);
	extensions.SetCount(extensionCount);

	// Get extension properties.
	RENDERER_SAFECALL(vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.Data()), "Renderer Error: Failed to obtain Vulkan extension properties");

	int requiredExtensionCount = 0; // Amount of required extensions found.

	// Compare required extension names with available extension names, find all matches.
	for (int i = 0; i < extensionNames.Count(); ++i)
	{
		for (int j = 0; j < extensions.Count(); ++j)
		{
			if (strcmp(extensionNames[i], extensions[j].extensionName) == 0)
			{
				++requiredExtensionCount;
				break;
			}
		}
	}

	return requiredExtensionCount == extensionNames.Count();
}

RendererHelper::SwapChainDetails* RendererHelper::GetSwapChainSupportDetails(VkSurfaceKHR windowSurface, VkPhysicalDevice device)
{
	SwapChainDetails* details = new SwapChainDetails;

	// Get surface capabilities.
	RENDERER_SAFECALL(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, windowSurface, &details->m_capabilities), "Renderer Error: Failed to obtain window surface capabilities.");

	// Get available surface format count.
	unsigned int formatCount = 0;
	RENDERER_SAFECALL(vkGetPhysicalDeviceSurfaceFormatsKHR(device, windowSurface, &formatCount, nullptr), "Renderer Error: Failed to obtain window surface format count.");

	// Get available swap chain formats.
	if (formatCount > 0)
	{
		details->m_formats.SetSize(formatCount);
		details->m_formats.SetCount(formatCount);

		RENDERER_SAFECALL(vkGetPhysicalDeviceSurfaceFormatsKHR(device, windowSurface, &formatCount, details->m_formats.Data()), "Renderer Error: Failed to obtain window surface formats.");
	}
	else
		throw std::runtime_error("Renderer Error: No supported formats found for provided window surface.");

	// Get present mode count.
	unsigned int presentModeCount = 0;
	RENDERER_SAFECALL(vkGetPhysicalDeviceSurfacePresentModesKHR(device, windowSurface, &presentModeCount, nullptr), "Renderer Error: Failed to obtain window surface present mode count.");

	// Get available swap chain present modes.
	if (presentModeCount > 0)
	{
		details->m_presentModes.SetSize(presentModeCount);
		details->m_presentModes.SetCount(presentModeCount);

		RENDERER_SAFECALL(vkGetPhysicalDeviceSurfacePresentModesKHR(device, windowSurface, &presentModeCount, details->m_presentModes.Data()), "Renderer Error: Failed to obtain window surface present modes.");
	}
	else
		throw std::runtime_error("Renderer Error: No supported present modes found for provided window surface.");

	return details;
}

RendererHelper::QueueFamilyIndices RendererHelper::FindQueueFamilies(VkSurfaceKHR windowSurface, VkPhysicalDevice device, RendererHelper::EQueueFamilyFlags eDesiredFamilies)
{
	unsigned int queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	// Get queue family information...
	DynamicArray<VkQueueFamilyProperties> queueFamilies(queueFamilyCount, 1);
	queueFamilies.SetCount(queueFamilyCount);

	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.Data());

	RendererHelper::QueueFamilyIndices outIndices = {};

	// Get queue indices.
	for (unsigned int i = 0; i < queueFamilyCount; ++i)
	{
		VkBool32 hasPresentSupport = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, windowSurface, &hasPresentSupport);

		// Find present family index.
		if (eDesiredFamilies & QUEUE_FAMILY_PRESENT && queueFamilies[i].queueCount > 0 && hasPresentSupport)
		{
			outIndices.m_nPresentFamilyIndex = i;

			outIndices.m_eFoundQueueFamilies = static_cast<EQueueFamilyFlags>(outIndices.m_eFoundQueueFamilies | QUEUE_FAMILY_PRESENT);
		}

		// Find graphics family index.
		if (eDesiredFamilies & QUEUE_FAMILY_GRAPHICS && queueFamilies[i].queueCount > 0 && queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			outIndices.m_nGraphicsFamilyIndex = i;

			outIndices.m_eFoundQueueFamilies = static_cast<EQueueFamilyFlags>(outIndices.m_eFoundQueueFamilies | QUEUE_FAMILY_GRAPHICS);
		}

		// Find compute family index.
		if (eDesiredFamilies & QUEUE_FAMILY_COMPUTE && queueFamilies[i].queueCount > 0 && queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
		{
			outIndices.m_nComputeFamilyIndex = i;

			outIndices.m_eFoundQueueFamilies = static_cast<EQueueFamilyFlags>(outIndices.m_eFoundQueueFamilies | QUEUE_FAMILY_COMPUTE);
		}

		// Find transfer queue family index, it should not be a graphics family.
		if (eDesiredFamilies & QUEUE_FAMILY_TRANSFER && queueFamilies[i].queueCount > 0 && queueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT && !(queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
		{
			outIndices.m_nTransferFamilyIndex = i;

			outIndices.m_eFoundQueueFamilies = static_cast<EQueueFamilyFlags>(outIndices.m_eFoundQueueFamilies | QUEUE_FAMILY_TRANSFER);
		}
	}

	outIndices.m_bAllFamiliesFound = (outIndices.m_eFoundQueueFamilies & eDesiredFamilies) == eDesiredFamilies;

	return outIndices;
}

int RendererHelper::DeviceSuitable(VkSurfaceKHR windowSurface, VkPhysicalDevice device, const DynamicArray<const char*>& extensionNames, EQueueFamilyFlags eDesiredQueueFamilies)
{
	if (windowSurface == VK_NULL_HANDLE)
		throw std::runtime_error("Renderer Error: Cannot check suitability for a null window surface!");

	if (device == VK_NULL_HANDLE)
		throw std::runtime_error("Renderer Error: Cannot check suitability for a null device!");

	// Get physical device properties & features.
	VkPhysicalDeviceProperties properties = {};
	vkGetPhysicalDeviceProperties(device, &properties);

	VkPhysicalDeviceFeatures features = {};
	vkGetPhysicalDeviceFeatures(device, &features);

	// Check if the provided extensions are supported.
	bool extensionsSupported = CheckDeviceExtensionSupport(extensionNames, device);

	if (!extensionsSupported)
		return 0;

	// Get swap chain information.
	SwapChainDetails* swapChainDetails = GetSwapChainSupportDetails(windowSurface, device);

	// Check if swap chain support is suitable.
	bool suitableSwapChain = swapChainDetails->m_formats.Count() > 0 && swapChainDetails->m_presentModes.Count() > 0;

	delete swapChainDetails;

	if (!suitableSwapChain)
		return 0;

	int score = properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && features.geometryShader;
	score += FindQueueFamilies(windowSurface, device, eDesiredQueueFamilies).m_bAllFamiliesFound; // Ensures all correct queue families are found.

	if (score == 0)
		return score;

	score += properties.limits.maxImageDimension2D;
	score += properties.limits.maxFramebufferWidth;
	score += properties.limits.maxFramebufferHeight;
	score += properties.limits.maxColorAttachments;
	score += properties.limits.maxMemoryAllocationCount;

	return score;
}

VkResult RendererHelper::CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* createInfo, const VkAllocationCallbacks* allocator, VkDebugUtilsMessengerEXT* messenger)
{
	auto func = GetExternalFunction<PFN_vkCreateDebugUtilsMessengerEXT>(instance, "vkCreateDebugUtilsMessengerEXT");

	if (func)
	{
		return func(instance, createInfo, allocator, messenger);
	}
	else
		return VK_ERROR_EXTENSION_NOT_PRESENT;
}

VkResult RendererHelper::DestroyDebugUtilsMessengerEXT(VkInstance instance, const VkAllocationCallbacks* allocator, VkDebugUtilsMessengerEXT* messenger)
{
	auto func = GetExternalFunction<PFN_vkDestroyDebugUtilsMessengerEXT>(instance, "vkDestroyDebugUtilsMessengerEXT");

	if (func)
	{
		func(instance, *messenger, allocator);

		return VK_SUCCESS;
	}
	else
		return VK_ERROR_EXTENSION_NOT_PRESENT;
}

VKAPI_ATTR VkBool32 VKAPI_CALL RendererHelper::ErrorCallback
(
	VkDebugUtilsMessageSeverityFlagBitsEXT severity,
	VkDebugUtilsMessageTypeFlagsEXT type,
	const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
	void* userData
)
{
	if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		std::cout << "Vulkan Validation Error: " << callbackData->pMessage << std::endl;
	else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		std::cout << "Vulkan Validation Warning: " << callbackData->pMessage << std::endl;
	else
		std::cout << "Vulkan Validation Info: " << callbackData->pMessage << std::endl;

	return VK_FALSE;
}

void RendererHelper::SetupDebugMessenger(const VkInstance& instance, VkDebugUtilsMessengerEXT& messenger)
{
	VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = &ErrorCallback;
	createInfo.pUserData = nullptr;

	RENDERER_SAFECALL(CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &messenger), "Renderer Error: Failed to create debug messenger!");
}