#pragma once
#define GLFW_INCLUDE_VULKAN
#include "glfw3.h"
#include "DynamicArray.h"

#define QUEUE_PRIORITY 1.0f

#define RENDERER_SAFECALL(func, message) Renderer::safeCallResult = func; if(Renderer::safeCallResult) { throw std::runtime_error(message); }

class Renderer
{
public:

	Renderer(GLFWwindow* window);

    ~Renderer();

	static VkResult safeCallResult;

private:

	static VKAPI_ATTR VkBool32 VKAPI_CALL ErrorCallback
	(
		VkDebugUtilsMessageSeverityFlagBitsEXT severity,
		VkDebugUtilsMessageTypeFlagsEXT type,
		const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
		void* userData
	);

	// Create Vulkan instance.
	void CreateVKInstance();

	// Check if the necessary validation layers are supported for debug.
	void CheckValidationLayerSupport();

	// Rate how suitable a device is for this renderer.
	int DeviceSuitable(VkPhysicalDevice device);

	// Get GPU to be used for rendering.
	void GetPhysicalDevice();

	// Debug messenger creation proxy function.
	VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* createInfo, const VkAllocationCallbacks* allocator, VkDebugUtilsMessengerEXT* messenger);

	// Debug messenger destruction proxy function.
	VkResult DestroyDebugUtilsMessengerEXT(VkInstance instance, const VkAllocationCallbacks* allocator, VkDebugUtilsMessengerEXT* messenger);

	// Create debug messenger.
	void SetupDebugMessenger();

	// Find supported queue families.
	bool FindQueueFamilies(VkPhysicalDevice device);

	// Create logical device to interface with the physical device.
	void CreateLogicalDevice();

	GLFWwindow* m_window;

	// Validation layers
	static const DynamicArray<const char*> m_validationLayers;
	static const bool m_enableValidationLayers;

	VkDebugUtilsMessengerEXT m_messenger;

	// Vulkan API
	VkInstance m_instance;
	VkPhysicalDevice m_physDevice;
	VkDevice m_logicDevice;

	VkQueue m_graphicsQueue;

	int m_graphicsQueueFamilyIndex;
	int m_computeQueueFamilyIndex;

	VkExtensionProperties* m_extensions;
	unsigned int m_extensionCount;
};

