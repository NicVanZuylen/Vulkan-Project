#include "Renderer.h"
#include <algorithm>
#include <set>
#include <thread>

#include "Shader.h"
#include "Material.h"
#include "MeshRenderer.h"
#include "gtc/matrix_transform.hpp"

#define GLFW_FORCE_RADIANS
#define GLFW_FORCE_DEPTH_ZERO_TO_ONE

const DynamicArray<const char*> Renderer::m_validationLayers = 
{
	"VK_LAYER_KHRONOS_validation"
};

const DynamicArray<const char*> Renderer::m_deviceExtensions =
{
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifndef RENDERER_DEBUG

const bool Renderer::m_enableValidationLayers = false;

#else

const bool Renderer::m_enableValidationLayers = true;

#endif

VkResult Renderer::m_safeCallResult = VK_SUCCESS;
DynamicArray<CopyRequest> Renderer::m_copyRequests;

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
	m_transferQueueFamilyIndex = -1;
	m_computeQueueFamilyIndex = -1;

	m_transferThread = new std::thread(&Renderer::SubmitTransferOperations, this);
	m_bTransferThread = true;
	m_bTransferReady = true;

	CheckValidationLayerSupport();
	CreateVKInstance();
	SetupDebugMessenger();
	CreateWindowSurface();
	GetPhysicalDevice();
	CreateLogicalDevice();
	CreateSwapChain();
	CreateSwapChainImageViews();

	// Get queue handles...
	vkGetDeviceQueue(m_logicDevice, m_presentQueueFamilyIndex, 0, &m_presentQueue);
	vkGetDeviceQueue(m_logicDevice, m_graphicsQueueFamilyIndex, 0, &m_graphicsQueue);
	vkGetDeviceQueue(m_logicDevice, m_transferQueueFamilyIndex, 0, &m_transferQueue);
	vkGetDeviceQueue(m_logicDevice, m_computeQueueFamilyIndex, 0, &m_computeQueue); // May also be the graphics queue.

	CreateRenderPasses();
	CreateMVPDescriptorSetLayout();
	CreateMVPUniformBuffers();
	CreateUBOMVPDescriptorPool();
	CreateUBOMVPDescriptorSets();
	CreateFramebuffers();
	CreateCommandPool();
	CreateSyncObjects();

	WaitGraphicsIdle();
	WaitTransferIdle();
}

Renderer::~Renderer()
{
	m_bTransferThread = false;
	m_transferThread->join();

	delete m_transferThread;
	m_transferThread = nullptr;

	vkDeviceWaitIdle(m_logicDevice);

	// Destroy descriptor pool.
	vkDestroyDescriptorPool(m_logicDevice, m_uboDescriptorPool, nullptr);

	// Destroy MVP Uniform buffers.
	for (int i = 0; i < m_mvpBuffers.Count(); ++i)
	{
		vkDestroyBuffer(m_logicDevice, m_mvpBuffers[i], nullptr);
		vkFreeMemory(m_logicDevice, m_mvpBufferMemBlocks[i], nullptr);
	}

	// Destroy MVP UBO descriptor set layout.
	vkDestroyDescriptorSetLayout(m_logicDevice, m_uboDescriptorSetLayout, nullptr);

	// Destroy semaphores.
	for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) 
	{
		vkDestroyFence(m_logicDevice, m_inFlightFences[i], nullptr);

	    vkDestroySemaphore(m_logicDevice, m_renderFinishedSemaphores[i], nullptr);
	    vkDestroySemaphore(m_logicDevice, m_imageAvailableSemaphores[i], nullptr);
	}

	for (int i = 0; i < MAX_CONCURRENT_COPIES; ++i) 
	{
		vkDestroyFence(m_logicDevice, m_copyReadyFences[i], nullptr);
	}

	// Destroy command pools.
	vkDestroyCommandPool(m_logicDevice, m_graphicsCmdPool, nullptr);
	vkDestroyCommandPool(m_logicDevice, m_transferCmdPool, nullptr);

	// Destroy framebuffers.
	for (int i = 0; i < m_swapChainFramebuffers.GetSize(); ++i)
		vkDestroyFramebuffer(m_logicDevice, m_swapChainFramebuffers[i], nullptr);

	// Destroy render passes.
	vkDestroyRenderPass(m_logicDevice, m_staticRenderPass, nullptr);
	vkDestroyRenderPass(m_logicDevice, m_dynamicRenderPass, nullptr);

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

void Renderer::RegisterShader(Shader* shader) 
{
	if (shader->m_registered) 
	{
		std::cout << "Renderer Warning: Attempting to register a shader that is already registered!\n";
		return;
	}

	const DynamicArray<char>& vertContents = shader->m_vertContents;
	const DynamicArray<char>& fragContents = shader->m_fragContents;

	// Create infos.
	VkShaderModuleCreateInfo vertCreateInfo = {};
	vertCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vertCreateInfo.codeSize = vertContents.GetSize();
	vertCreateInfo.pCode = reinterpret_cast<const uint32_t*>(vertContents.Data());

	VkShaderModuleCreateInfo fragCreateInfo = {};
	fragCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	fragCreateInfo.codeSize = fragContents.GetSize();
	fragCreateInfo.pCode = reinterpret_cast<const uint32_t*>(fragContents.Data());

	// Create modules.
	RENDERER_SAFECALL(vkCreateShaderModule(m_logicDevice, &vertCreateInfo, nullptr, &shader->m_vertModule), "Renderer Error: Failed to create vertex shader module.");
	RENDERER_SAFECALL(vkCreateShaderModule(m_logicDevice, &fragCreateInfo, nullptr, &shader->m_fragModule), "Renderer Error: Failed to create fragment shader module.");

	// Shader is now registered.
	shader->m_registered = true;
}

void Renderer::UnregisterShader(Shader* shader) 
{
	if(shader->m_registered) 
	{
		// Wait for frames in flight to finish execution.
		vkWaitForFences(m_logicDevice, m_inFlightFences.GetSize(), m_inFlightFences.Data(), VK_TRUE, std::numeric_limits<unsigned long long>::max());

		// Destroy modules...
		vkDestroyShaderModule(m_logicDevice, shader->m_vertModule, nullptr);
		vkDestroyShaderModule(m_logicDevice, shader->m_fragModule, nullptr);

		// Shader is no longer registered.
		shader->m_registered = false;
	}
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

	std::cout << "\n";
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

	// Get queue family information...
	DynamicArray<VkQueueFamilyProperties> queueFamilies(queueFamilyCount, 1);
	queueFamilies.SetCount(queueFamilyCount);

	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.Data());

	// Get queue indices.
	for(unsigned int i = 0; i < queueFamilyCount; ++i) 
	{
		VkBool32 hasPresentSupport = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_windowSurface, &hasPresentSupport);

		// Find present family index.
		if (queueFamilies[i].queueCount > 0 && hasPresentSupport)
		{
			m_presentQueueFamilyIndex = i;
		}

		// Find graphics family index.
		if (queueFamilies[i].queueCount > 0 && queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			m_graphicsQueueFamilyIndex = i;
		}

		// Find compute family index.
		if (queueFamilies[i].queueCount > 0 && queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
		{
			m_computeQueueFamilyIndex = i;
		}

		// Find transfer queue family index, it should not be a graphics family.
		if (queueFamilies[i].queueCount > 0 && queueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT && !(queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
		{
			m_transferQueueFamilyIndex = i;
		}
	}

	return m_presentQueueFamilyIndex > -1 && m_graphicsQueueFamilyIndex > -1 && m_computeQueueFamilyIndex > -1 && m_transferQueueFamilyIndex > -1;
}

void Renderer::CreateLogicalDevice()
{
	DynamicArray<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<int> uniqueQueueFamilies = { m_presentQueueFamilyIndex, m_graphicsQueueFamilyIndex, m_computeQueueFamilyIndex, m_transferQueueFamilyIndex };

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
	features.samplerAnisotropy = VK_TRUE;

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
	m_swapChainImageViews.SetSize(m_swapChainImages.GetSize());
	m_swapChainImageViews.SetCount(m_swapChainImageViews.GetSize());

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
		viewCreateInfo.subresourceRange.layerCount = 1;

		RENDERER_SAFECALL(vkCreateImageView(m_logicDevice, &viewCreateInfo, nullptr, &view), "Renderer Error: Failed to create swap chain image views.");

		m_swapChainImageViews[i] = view;
	}
}

void Renderer::CreateRenderPasses() 
{
	// ------------------------------------------------------------------------------------------------------------
	// Static object pass

	{
		VkAttachmentDescription colorAttachment = {};
		colorAttachment.format = m_swapChainImageFormat;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT; // No multisampling.
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // Clear buffer initially.
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // Store for use in a future render pass.
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // Don't care.
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // Don't care.
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Initial layout is unknown.
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // Output optimal layout for the next render pass.

		VkAttachmentReference colorAttachmentRef = {};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // Expected optimal layout.

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1; // One attachment.
		subpass.pColorAttachments = &colorAttachmentRef;

		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &colorAttachment;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;

		VkSubpassDependency dependency = {};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // Ensures the source color is outputted from the pipeline, meaning processing has finished.
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		RENDERER_SAFECALL(vkCreateRenderPass(m_logicDevice, &renderPassInfo, nullptr, &m_staticRenderPass), "Renderer Error: Failed to create render pass.");
	}

	// ------------------------------------------------------------------------------------------------------------
	// Dynamic object pass

	{
		VkAttachmentDescription colorAttachment = {};
		colorAttachment.format = m_swapChainImageFormat;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT; // Not multisampling right now.
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; // Load attachment from store.
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // Store color attachment for presentation.
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // Don't care.
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // Don't care.
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // Expecting optimal color attachment layout.
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // Output layout should be presentable to the screen.

		VkAttachmentReference colorAttachmentRef = {};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // Expected optimal layout.

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;

		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1; // One attachment.
		renderPassInfo.pAttachments = &colorAttachment;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;

		VkSubpassDependency dependency = {};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // Ensures the source color is outputted from the pipeline, meaning processing has finished.
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		RENDERER_SAFECALL(vkCreateRenderPass(m_logicDevice, &renderPassInfo, nullptr, &m_dynamicRenderPass), "Renderer Error: Failed to create render pass.");
	}

	// ------------------------------------------------------------------------------------------------------------
}

void Renderer::CreateMVPDescriptorSetLayout() 
{
	VkDescriptorSetLayoutBinding uboLayoutBinding = {};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	uboLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutCreateInfo.bindingCount = 1;
	layoutCreateInfo.pBindings = &uboLayoutBinding;
	layoutCreateInfo.pNext = nullptr;

	RENDERER_SAFECALL(vkCreateDescriptorSetLayout(m_logicDevice, &layoutCreateInfo, nullptr, &m_uboDescriptorSetLayout), "Renderer Error: Failed to create MVP UBO descriptor set layout.");
}

void Renderer::CreateMVPUniformBuffers() 
{
	unsigned int bufferSize = sizeof(MVPUniformBuffer);

	// Resize arrays.
	m_mvpBuffers.SetSize(m_swapChainImageViews.Count());
	m_mvpBuffers.SetCount(m_mvpBuffers.GetSize());

	m_mvpBufferMemBlocks.SetSize(m_mvpBuffers.GetSize());
	m_mvpBufferMemBlocks.SetCount(m_mvpBufferMemBlocks.GetSize());

	for (int i = 0; i < m_mvpBuffers.Count(); ++i)
		CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_mvpBuffers[i], m_mvpBufferMemBlocks[i]);
}

void Renderer::CreateUBOMVPDescriptorPool()
{
	VkDescriptorPoolSize poolSize = {};
	poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSize.descriptorCount = static_cast<uint32_t>(m_swapChainImages.GetSize());

	VkDescriptorPoolCreateInfo poolCreateInfo = {};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCreateInfo.poolSizeCount = 1;
	poolCreateInfo.pPoolSizes = &poolSize;
	poolCreateInfo.maxSets = static_cast<uint32_t>(m_swapChainImages.GetSize());

	RENDERER_SAFECALL(vkCreateDescriptorPool(m_logicDevice, &poolCreateInfo, nullptr, &m_uboDescriptorPool), "Renderer Error: Failed to create MVP UBO descriptor pool.");
}

void Renderer::CreateUBOMVPDescriptorSets() 
{
	// Descriptor layouts for each swap chain image
	DynamicArray<VkDescriptorSetLayout> layouts;
	for (int i = 0; i < m_swapChainImageViews.GetSize(); ++i)
		layouts.Push(m_uboDescriptorSetLayout);

	// Allocation info for descriptor sets.
	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = m_uboDescriptorPool;
	allocInfo.descriptorSetCount = layouts.GetSize();
	allocInfo.pSetLayouts = layouts.Data();

	// Resize descriptor set array.
	m_uboDescriptorSets.SetSize(m_swapChainImageViews.GetSize());
	m_uboDescriptorSets.SetCount(m_uboDescriptorSets.GetSize());

	// Allocate sets, like command pools destroying the descriptor pool destroys the sets,
	// so no explicit action is needed to free the descriptor sets.
	RENDERER_SAFECALL(vkAllocateDescriptorSets(m_logicDevice, &allocInfo, m_uboDescriptorSets.Data()), "Renderer Error: Failed to allocate MVP UBO descriptor sets.");

	for(int i = 0; i < m_uboDescriptorSets.GetSize(); ++i) 
	{
		VkDescriptorBufferInfo bufferInfo = {};
		bufferInfo.buffer = m_mvpBuffers[i];
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(MVPUniformBuffer);

		VkWriteDescriptorSet descWriteInfo = {};
		descWriteInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descWriteInfo.dstSet = m_uboDescriptorSets[i];
		descWriteInfo.dstBinding = 0;
		descWriteInfo.dstArrayElement = 0;
		descWriteInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descWriteInfo.descriptorCount = 1;
		descWriteInfo.pBufferInfo = &bufferInfo;
		descWriteInfo.pImageInfo = nullptr;
		descWriteInfo.pTexelBufferView = nullptr;

		// Update descriptor sets to use the uniform buffers.
		vkUpdateDescriptorSets(m_logicDevice, 1, &descWriteInfo, 0, nullptr);
	}
}

void Renderer::CreateFramebuffers() 
{
	m_swapChainFramebuffers.SetSize(m_swapChainImageViews.GetSize());
	m_swapChainFramebuffers.SetCount(m_swapChainImageViews.GetSize());

	for (int i = 0; i < m_swapChainImageViews.Count(); ++i) 
	{
		VkImageView attachment = m_swapChainImageViews[i]; // Image used as the framebuffer attachment.

		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = m_dynamicRenderPass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = &attachment;
		framebufferInfo.width = m_swapChainImageExtents.width;
		framebufferInfo.height = m_swapChainImageExtents.height;
		framebufferInfo.layers = 1;

		RENDERER_SAFECALL(vkCreateFramebuffer(m_logicDevice, &framebufferInfo, nullptr, &m_swapChainFramebuffers[i]), "Renderer Error: Failed to create swap chain framebuffer.");
	}
}

void Renderer::CreateCommandPool() 
{
	// ==================================================================================================================================================
	// Graphics commands

	VkCommandPoolCreateInfo poolCreateInfo = {};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolCreateInfo.queueFamilyIndex = m_graphicsQueueFamilyIndex;
	poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

	RENDERER_SAFECALL(vkCreateCommandPool(m_logicDevice, &poolCreateInfo, nullptr, &m_graphicsCmdPool), "Renderer Error: Failed to create command pool.");

	// Allocate memory for static pass command buffers...
	m_staticPassCmdBufs.SetSize(m_swapChainFramebuffers.GetSize());
	m_staticPassCmdBufs.SetCount(m_staticPassCmdBufs.GetSize());

	// Allocation info.
	VkCommandBufferAllocateInfo cmdBufAllocateInfo = {};
	cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdBufAllocateInfo.commandPool = m_graphicsCmdPool;
	cmdBufAllocateInfo.commandBufferCount = m_staticPassCmdBufs.GetSize();
	cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	// Allocate static command buffers.
	RENDERER_SAFECALL(vkAllocateCommandBuffers(m_logicDevice, &cmdBufAllocateInfo, m_staticPassCmdBufs.Data()), "Renderer Error: Failed to allocate static command buffers.");

	VkCommandBufferBeginInfo cmdBeginInfo = {};
	cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	cmdBeginInfo.pInheritanceInfo = nullptr;

	VkClearValue clearVal = { 0.0f, 0.0f, 0.0f, 1.0f };

	// Record static command buffers...
	for(int i = 0; i < m_staticPassCmdBufs.Count(); ++i) 
	{
		VkCommandBuffer& cmdBuffer = m_staticPassCmdBufs[i];

		RENDERER_SAFECALL(vkBeginCommandBuffer(cmdBuffer, &cmdBeginInfo), "Renderer Error: Failed to begin recording of static pass command buffer.");

		// Begin render pass.
		VkRenderPassBeginInfo passBeginInfo = {};
		passBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		passBeginInfo.renderPass = m_staticRenderPass;
		passBeginInfo.framebuffer = m_swapChainFramebuffers[i];
		passBeginInfo.renderArea.offset = { 0, 0 };
		passBeginInfo.renderArea.extent = m_swapChainImageExtents;

		passBeginInfo.clearValueCount = 1;
		passBeginInfo.pClearValues = &clearVal;

		vkCmdBeginRenderPass(cmdBuffer, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		// Static objects in the scene will be drawn here...

		vkCmdEndRenderPass(cmdBuffer);

		RENDERER_SAFECALL(vkEndCommandBuffer(cmdBuffer), "Renderer Error: Failed to end recording of static pass command buffer.");
	}

	// Allocate memory for dynamic command buffers...
	m_dynamicPassCmdBufs.SetSize(m_swapChainFramebuffers.GetSize());
	m_dynamicPassCmdBufs.SetCount(m_dynamicPassCmdBufs.GetSize());

	// Allocate dynamic command buffers...
	RENDERER_SAFECALL(vkAllocateCommandBuffers(m_logicDevice, &cmdBufAllocateInfo, m_dynamicPassCmdBufs.Data()), "Renderer Error: Failed to allocate dynamic command buffers.");

	// Initial recording of dynamic command buffers...
	for (int i = 0; i < m_dynamicPassCmdBufs.Count(); ++i)
	{
		VkCommandBuffer& cmdBuffer = m_dynamicPassCmdBufs[i];

		RENDERER_SAFECALL(vkBeginCommandBuffer(cmdBuffer, &cmdBeginInfo), "Renderer Error: Failed to begin recording of static pass command buffer.");

		// Begin render pass.
		VkRenderPassBeginInfo passBeginInfo = {};
		passBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		passBeginInfo.renderPass = m_dynamicRenderPass;
		passBeginInfo.framebuffer = m_swapChainFramebuffers[i];
		passBeginInfo.renderArea.offset = { 0, 0 };
		passBeginInfo.renderArea.extent = m_swapChainImageExtents;
		passBeginInfo.clearValueCount = 0;
		passBeginInfo.pClearValues = nullptr;

		vkCmdBeginRenderPass(cmdBuffer, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		DynamicArray<PipelineData*>& allPipelines = MeshRenderer::Pipelines();

		// Iterate through all pipelines and draw their objects.
		for(int i = 0; i < allPipelines.Count(); ++i) 
		{
			// Bind pipelines...
			vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, allPipelines[i]->m_handle);

			// Draw objects using the pipeline.
			DynamicArray<MeshRenderer*>& renderObjects = allPipelines[i]->m_renderObjects;
			for(int j = 0; j < renderObjects.Count(); ++j) 
			{
				renderObjects[j]->CommandDraw(cmdBuffer);
			}
		}

		vkCmdEndRenderPass(cmdBuffer);

		RENDERER_SAFECALL(vkEndCommandBuffer(cmdBuffer), "Renderer Error: Failed to end recording of static pass command buffer.");
	}

	// Flag that there is no need to re-record the dynamic command buffers.
	m_dynamicStateChange[0] = false;
	m_dynamicStateChange[1] = false;
	m_dynamicStateChange[2] = false;

	// ==================================================================================================================================================
	// Transfer commands

	VkCommandPoolCreateInfo transferPoolInfo = {};
	transferPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	transferPoolInfo.queueFamilyIndex = m_transferQueueFamilyIndex;
	transferPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

	RENDERER_SAFECALL(vkCreateCommandPool(m_logicDevice, &transferPoolInfo, nullptr, &m_transferCmdPool), "Renderer Error: Failed to create dedicated transfer command pool.");

	VkCommandBufferAllocateInfo transAllocInfo = {};
	transAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	transAllocInfo.commandPool = m_transferCmdPool;
	transAllocInfo.commandBufferCount = MAX_CONCURRENT_COPIES;
	transAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	m_transferCmdBufs.SetSize(MAX_CONCURRENT_COPIES);
	m_transferCmdBufs.SetCount(MAX_CONCURRENT_COPIES);

	RENDERER_SAFECALL(vkAllocateCommandBuffers(m_logicDevice, &transAllocInfo, m_transferCmdBufs.Data()), "Renderer Error: Failed to create dedicated transfer command buffer.");
}

void Renderer::UpdateMVP(const unsigned int& bufferIndex)
{
	// Set up matrices...
	m_mvp.model = glm::mat4();
	//m_mvp.view = glm::lookAt(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f), glm::vec3(0.0f, -1.0f, 0.0f));
	m_mvp.proj = glm::perspective(45.0f, (float)m_swapChainImageExtents.width / (float)m_swapChainImageExtents.height, 0.1f, 1000.0f);

	// Change coordinate system using this matrix.
	glm::mat4 axisCorrection; // Inverts the Y axis to match the OpenGL coordinate system.
	axisCorrection[1][1] = -1.0f;
	axisCorrection[2][2] = 1.0f;
	axisCorrection[3][3] = 1.0f;

	m_mvp.proj = axisCorrection * m_mvp.proj;

	unsigned int bufferSize = sizeof(m_mvp);

	// Update buffer.
	void* buffer = nullptr;
	vkMapMemory(m_logicDevice, m_mvpBufferMemBlocks[bufferIndex], 0, bufferSize, 0, &buffer);

	memcpy_s(buffer, bufferSize, &m_mvp, bufferSize);

	vkUnmapMemory(m_logicDevice, m_mvpBufferMemBlocks[bufferIndex]);
}

void Renderer::RecordTransferCommandBuffer(const unsigned int& bufferIndex)
{
	if (m_copyRequests.Count() == 0 || !m_bTransferReady)
		return;

	VkCommandBuffer& cmdBuffer = m_transferCmdBufs[bufferIndex];

	VkCommandBufferBeginInfo cmdBeginInfo = {};
	cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	cmdBeginInfo.pInheritanceInfo = nullptr;

	RENDERER_SAFECALL(vkBeginCommandBuffer(cmdBuffer, &cmdBeginInfo), "Renderer Error: Failed to begin recording of transfer command buffer.");

	// Issue commands for each requested copy operation.
	for (int i = 0; i < m_copyRequests.Count(); ++i) 
	{
		vkCmdCopyBuffer(cmdBuffer, m_copyRequests[i].srcBuffer, m_copyRequests[i].dstBuffer, 1, &m_copyRequests[i].copyRegion);
	}

	RENDERER_SAFECALL(vkEndCommandBuffer(cmdBuffer), "Renderer Error: Failed to end recording of transfer command buffer.");
	m_copyRequests.Clear();

	m_bTransferReady = false;
}

void Renderer::RecordDynamicCommandBuffer(const unsigned int& bufferIndex)
{
	// If there was no dynamic state change, no re-recording is necessary.
	if (!m_dynamicStateChange[bufferIndex])
		return;

	VkCommandBufferBeginInfo cmdBeginInfo = {};
	cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	cmdBeginInfo.pInheritanceInfo = nullptr;

	// Initial recording of dynamic command buffers...
	VkCommandBuffer& cmdBuffer = m_dynamicPassCmdBufs[bufferIndex];

	RENDERER_SAFECALL(vkBeginCommandBuffer(cmdBuffer, &cmdBeginInfo), "Renderer Error: Failed to begin recording of static pass command buffer.");

	// Begin render pass.
	VkRenderPassBeginInfo passBeginInfo = {};
	passBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	passBeginInfo.renderPass = m_dynamicRenderPass;
	passBeginInfo.framebuffer = m_swapChainFramebuffers[bufferIndex];
	passBeginInfo.renderArea.offset = { 0, 0 };
	passBeginInfo.renderArea.extent = m_swapChainImageExtents;
	passBeginInfo.clearValueCount = 0;
	passBeginInfo.pClearValues = nullptr;

	vkCmdBeginRenderPass(cmdBuffer, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	DynamicArray<PipelineData*>& allPipelines = MeshRenderer::Pipelines();

	// Iterate through all pipelines and draw their objects.
	for (int i = 0; i < allPipelines.Count(); ++i)
	{
		PipelineData& currentPipeline = *allPipelines[i];

		// Bind pipelines...
		vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, currentPipeline.m_handle);

		currentPipeline.m_material->UseDescriptorSet(cmdBuffer, currentPipeline.m_layout, bufferIndex);

		// Draw objects using the pipeline.
		DynamicArray<MeshRenderer*>& renderObjects = currentPipeline.m_renderObjects;
		for (int j = 0; j < renderObjects.Count(); ++j)
		{
			renderObjects[j]->UpdateInstanceData();
			renderObjects[j]->CommandDraw(cmdBuffer);
		}
	}

	vkCmdEndRenderPass(cmdBuffer);

	RENDERER_SAFECALL(vkEndCommandBuffer(cmdBuffer), "Renderer Error: Failed to end recording of static pass command buffer.");

	// Flag that there is no further dynamic state changes yet.
	m_dynamicStateChange[bufferIndex] = false;
}

void Renderer::CreateSyncObjects() 
{
	m_imageAvailableSemaphores.SetSize(MAX_FRAMES_IN_FLIGHT);
	m_imageAvailableSemaphores.SetCount(MAX_FRAMES_IN_FLIGHT);

	m_renderFinishedSemaphores.SetSize(MAX_FRAMES_IN_FLIGHT);
	m_renderFinishedSemaphores.SetCount(MAX_FRAMES_IN_FLIGHT);

	m_copyReadyFences.SetSize(MAX_CONCURRENT_COPIES);
	m_copyReadyFences.SetCount(MAX_CONCURRENT_COPIES);

	m_inFlightFences.SetSize(MAX_FRAMES_IN_FLIGHT);
	m_inFlightFences.SetCount(MAX_FRAMES_IN_FLIGHT);

	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	// Create rendering semaphores.
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) 
	{
		RENDERER_SAFECALL(vkCreateSemaphore(m_logicDevice, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]), "Renderer Error: Failed to create semaphores.");
		RENDERER_SAFECALL(vkCreateSemaphore(m_logicDevice, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]), "Renderer Error: Failed to create semaphores.");
		
		RENDERER_SAFECALL(vkCreateFence(m_logicDevice, &fenceInfo, nullptr, &m_inFlightFences[i]), "Renderer Error: Failed to create in-flight fence.");
	}

	// Create transfer semaphores.
	for(int i = 0; i < MAX_CONCURRENT_COPIES; ++i) 
	{
		RENDERER_SAFECALL(vkCreateFence(m_logicDevice, &fenceInfo, nullptr, &m_copyReadyFences[i]), "Renderer Error: Failed to create semaphores.");
	}
}

void Renderer::SubmitTransferOperations() 
{
	while(m_bTransferThread) 
	{
		//std::cout << "Noice." << std::endl;

		if (!m_bTransferReady)
		{
			//std::cout << "Transfering..." << std::endl;

			unsigned int currentCmdIndex = m_currentFrame % MAX_CONCURRENT_COPIES;

			VkSubmitInfo transSubmitInfo = {};
			transSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			transSubmitInfo.commandBufferCount = 1;
			transSubmitInfo.waitSemaphoreCount = 0;
			transSubmitInfo.pWaitSemaphores = nullptr;
			transSubmitInfo.signalSemaphoreCount = 0;
			transSubmitInfo.pSignalSemaphores = nullptr;
			transSubmitInfo.pCommandBuffers = &m_transferCmdBufs[0];

			vkWaitForFences(m_logicDevice, 1, &m_copyReadyFences[0], VK_TRUE, std::numeric_limits<unsigned long long>::max());
			vkResetFences(m_logicDevice, 1, &m_copyReadyFences[0]);

			// Record new transfer commands.
			//RecordTransferCommandBuffer(0);

			// Execute transfer commands.
			vkQueueSubmit(m_transferQueue, 1, &transSubmitInfo, m_copyReadyFences[0]);

			m_bTransferReady = true;
		}
	}
}

void Renderer::Begin() 
{
	m_currentFrameIndex = ++m_currentFrame % MAX_FRAMES_IN_FLIGHT;

	// Wait of frames-in-flight.
	vkWaitForFences(m_logicDevice, 1, &m_inFlightFences[m_currentFrameIndex], VK_TRUE, std::numeric_limits<unsigned long long>::max());
	vkResetFences(m_logicDevice, 1, &m_inFlightFences[m_currentFrameIndex]);

	// Aquire next image from the swap chain.
	RENDERER_SAFECALL(vkAcquireNextImageKHR(m_logicDevice, m_swapChain, std::numeric_limits<unsigned long long>::max(), m_imageAvailableSemaphores[m_currentFrameIndex], VK_NULL_HANDLE, &m_presentImageIndex),
		"Renderer Error: Failed to aquire next swap chain image.");
}

void Renderer::SubmitCopyOperation(VkCommandBuffer commandBuffer) 
{
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 0;
	submitInfo.pWaitSemaphores = nullptr;
	submitInfo.signalSemaphoreCount = 0;
	submitInfo.pSignalSemaphores = nullptr;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	RENDERER_SAFECALL(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE), "Renderer Error: Failed to submit copy operation to the GPU.");

	// Ensures the copy operation is complete before returning.
	vkQueueWaitIdle(m_graphicsQueue);
}

void Renderer::RequestCopy(const CopyRequest& request) 
{
	m_copyRequests.Push(request);
}

void Renderer::AddDynamicObject(MeshRenderer* object) 
{
	m_dynamicObjects.Push(object);

	m_dynamicStateChange[0] = true;
	m_dynamicStateChange[1] = true;
	m_dynamicStateChange[2] = true;
}

void Renderer::ForceDynamicStateChange() 
{
	m_dynamicStateChange[0] = true;
	m_dynamicStateChange[1] = true;
	m_dynamicStateChange[2] = true;
}

void Renderer::End() 
{
	UpdateMVP(m_presentImageIndex);

	// Re-record the dynamic command buffer for this swap chain image if it has changed.
    RecordDynamicCommandBuffer(m_presentImageIndex);

	/*
	if (m_copyRequests.Count() > 0)
	{
		unsigned int currentCmdIndex = m_currentFrame % MAX_CONCURRENT_COPIES;

		VkSubmitInfo transSubmitInfo = {};
		transSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		transSubmitInfo.commandBufferCount = 1;
		transSubmitInfo.waitSemaphoreCount = m_currentFrame / MAX_CONCURRENT_COPIES > 0;
		transSubmitInfo.pWaitSemaphores = &m_copyReadySemaphores[currentCmdIndex];
		transSubmitInfo.signalSemaphoreCount = 1;
		transSubmitInfo.pSignalSemaphores = &m_copyReadySemaphores[currentCmdIndex];
		transSubmitInfo.pCommandBuffers = &m_transferCmdBufs[currentCmdIndex];

		// Record new transfer commands.
		RecordTransferCommandBuffer(currentCmdIndex);

		// Execute transfer commands.
		vkQueueSubmit(m_transferQueue, 1, &transSubmitInfo, VK_NULL_HANDLE);
	}
	*/

	if (m_copyRequests.Count() > 0)
		RecordTransferCommandBuffer(0);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	VkCommandBuffer cmdBuffers[] = { m_staticPassCmdBufs[m_presentImageIndex], m_dynamicPassCmdBufs[m_presentImageIndex] };

	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &m_imageAvailableSemaphores[m_currentFrameIndex];
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 2;
	submitInfo.pCommandBuffers = cmdBuffers;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &m_renderFinishedSemaphores[m_currentFrameIndex];

	RENDERER_SAFECALL(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFences[m_currentFrameIndex]), "Renderer Error: Failed to submit command buffer.");

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &m_renderFinishedSemaphores[m_currentFrameIndex];
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &m_swapChain;
	presentInfo.pImageIndices = &m_presentImageIndex;
	presentInfo.pResults = nullptr;

	RENDERER_SAFECALL(vkQueuePresentKHR(m_graphicsQueue, &presentInfo), "Renderer Error: Failed to present swap chain image.");
}

void Renderer::WaitGraphicsIdle() 
{
	vkQueueWaitIdle(m_graphicsQueue);
}

void Renderer::WaitTransferIdle() 
{
	vkQueueWaitIdle(m_transferQueue);
}

unsigned int Renderer::FindMemoryType(unsigned int typeFilter, VkMemoryPropertyFlags propertyFlags)
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(m_physDevice, &memProperties);

	for (unsigned int i = 0; i < memProperties.memoryTypeCount; ++i)
	{
		// Ensure properties match...
		if ((typeFilter & (1 << i)) && ((memProperties.memoryTypes[i].propertyFlags & propertyFlags) == propertyFlags))
		{
			return i;
		}
	}

	throw std::exception("Mesh Error: Failed to find suitable memory type for buffer allocation.");
	return 0;
}

void Renderer::CreateBuffer(const unsigned long long& size, const VkBufferUsageFlags& bufferUsage, VkMemoryPropertyFlags properties, VkBuffer& bufferHandle, VkDeviceMemory& bufferMemory)
{
	VkBufferCreateInfo bufCreateInfo = {};
	bufCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufCreateInfo.size = size;
	bufCreateInfo.usage = bufferUsage;
	bufCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	// Create buffer object.
	RENDERER_SAFECALL(vkCreateBuffer(m_logicDevice, &bufCreateInfo, nullptr, &bufferHandle), "Mesh Error: Failed to create buffer.");

	// Get memory requirements for the buffer.
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(m_logicDevice, bufferHandle, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);
	allocInfo.pNext = nullptr;

	RENDERER_SAFECALL(vkAllocateMemory(m_logicDevice, &allocInfo, nullptr, &bufferMemory), "Mesh Error: Failed to allocate buffer memory.");

	// Associate the newly allocated memory with the buffer object.
	vkBindBufferMemory(m_logicDevice, bufferHandle, bufferMemory, 0);
}

Renderer::TempCmdBuffer Renderer::CreateTempCommandBuffer()
{
	TempCmdBuffer tempBuffer;

	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = m_graphicsCmdPool;
	allocInfo.commandBufferCount = 1;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.pNext = nullptr;

	RENDERER_SAFECALL(vkAllocateCommandBuffers(m_logicDevice, &allocInfo, &tempBuffer.m_handle), "Renderer Error: Failed to allocate temporary command buffer.");

	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = 0;

	RENDERER_SAFECALL(vkCreateFence(m_logicDevice, &fenceCreateInfo, nullptr, &tempBuffer.m_destroyFence), "Renderer Error: Failed to create temporary command buffer execution fence.");

	return tempBuffer;
}

void Renderer::UseAndDestroyTempCommandBuffer(Renderer::TempCmdBuffer& buffer) 
{
	VkSubmitInfo bufferSubmitInfo = {};
	bufferSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	bufferSubmitInfo.pCommandBuffers = &buffer.m_handle;
	bufferSubmitInfo.commandBufferCount = 1;
	bufferSubmitInfo.pSignalSemaphores = nullptr;
	bufferSubmitInfo.pWaitSemaphores = nullptr;
	bufferSubmitInfo.waitSemaphoreCount = 0;
	bufferSubmitInfo.pWaitDstStageMask = nullptr;

	RENDERER_SAFECALL(vkQueueSubmit(m_graphicsQueue, 1, &bufferSubmitInfo, buffer.m_destroyFence), "Renderer Error: Failed to submit temporary command buffer for executino.");

	RENDERER_SAFECALL(vkWaitForFences(m_logicDevice, 1, &buffer.m_destroyFence, VK_TRUE, std::numeric_limits<unsigned long long>::max()), "Renderer Error: Failed to wait for temp command buffer fence.");

	vkDestroyFence(m_logicDevice, buffer.m_destroyFence, nullptr);
	vkFreeCommandBuffers(m_logicDevice, m_graphicsCmdPool, 1, &buffer.m_handle);
}

VkDevice Renderer::GetDevice() 
{
	return m_logicDevice;
}

VkPhysicalDevice Renderer::GetPhysDevice() 
{
	return m_physDevice;
}

VkCommandPool Renderer::GetCommandPool() 
{
	return m_graphicsCmdPool;
}

VkRenderPass Renderer::DynamicRenderPass() 
{
	return m_dynamicRenderPass;
}

VkDescriptorSetLayout Renderer::MVPUBOSetLayout() 
{
	return m_uboDescriptorSetLayout;
}

VkBuffer Renderer::MVPUBOHandle(const unsigned int& nSwapChainImageIndex)
{
	return m_mvpBuffers[nSwapChainImageIndex];
}

const DynamicArray<VkFramebuffer>& Renderer::GetFramebuffers() const
{
	return m_swapChainFramebuffers;
}

const unsigned int& Renderer::FrameWidth() const
{
	return m_swapChainImageExtents.width;
}

const unsigned int& Renderer::FrameHeight() const
{
	return m_swapChainImageExtents.height;
}

const unsigned int Renderer::SwapChainImageCount() const
{
	return m_swapChainImageViews.GetSize();
}

void Renderer::SetViewMatrix(glm::mat4& viewMat) 
{
	m_mvp.view = viewMat;
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