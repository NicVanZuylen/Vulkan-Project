#include "Renderer.h"
#include <stdexcept>
#include <iostream>

#define GLFW_FORCE_RADIANS
#define GLFW_FORCE_DEPTH_ZERO_TO_ONE

#include "glm.hpp"

const DynamicArray<const char*> Renderer::m_validationLayers = 
{
	"VK_LAYER_KHRONOS_validation",
};

VkResult Renderer::safeCallResult = VK_SUCCESS;

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

	m_instance = VK_NULL_HANDLE;
	m_physDevice = VK_NULL_HANDLE;

	m_graphicsQueueFamilyIndex = -1;
	m_computeQueueFamilyIndex = -1;

	CheckValidationLayerSupport();
	CreateVKInstance();
	SetupDebugMessenger();
	GetPhysicalDevice();
	CreateLogicalDevice();

	// Get queue handles...
	vkGetDeviceQueue(m_logicDevice, m_graphicsQueueFamilyIndex, 0, &m_graphicsQueue);
}

Renderer::~Renderer()
{
	delete[] m_extensions;

	// Destroy debug messenger.
	DestroyDebugUtilsMessengerEXT(m_instance, nullptr, &m_messenger);

	// Destroy logical device.
	vkDestroyDevice(m_logicDevice, nullptr);

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

	createInfo.enabledLayerCount = 0;

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

	// A device is suitable if it is a GPU with geometry shader and tesselation shader support.
	int score = properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && FindQueueFamilies(device) && features.geometryShader && features.tessellationShader;

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

	// Ensure a graphics queue family exists.
	for(unsigned int i = 0; i < queueFamilyCount; ++i) 
	{
		if (queueFamilies[i].queueCount > 0 && queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			m_graphicsQueueFamilyIndex = i;
		}
	}

	// Ensure a compute family exists.
	for (unsigned int i = 0; i < queueFamilyCount; ++i)
	{
		if (queueFamilies[i].queueCount > 0 && queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
		{
			m_computeQueueFamilyIndex = i;
		}
	}

	return m_graphicsQueueFamilyIndex > -1 && m_computeQueueFamilyIndex > -1;
}

void Renderer::CreateLogicalDevice()
{
	VkDeviceQueueCreateInfo queueCreateInfo = {};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.queueFamilyIndex = m_graphicsQueueFamilyIndex;
	queueCreateInfo.queueCount = 1;

	float queuePriority = QUEUE_PRIORITY;
	queueCreateInfo.pQueuePriorities = &queuePriority;

	VkPhysicalDeviceFeatures features = {};
	vkGetPhysicalDeviceFeatures(m_physDevice, &features);

	VkDeviceCreateInfo logicDeviceCreateInfo = {};
	logicDeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	logicDeviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
	logicDeviceCreateInfo.queueCreateInfoCount = 1;
	logicDeviceCreateInfo.pEnabledFeatures = &features;

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