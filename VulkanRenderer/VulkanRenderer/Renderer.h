#pragma once
#define GLFW_INCLUDE_VULKAN
#include "glfw3.h"

#include "DynamicArray.h"
#include "Table.h"

#define QUEUE_PRIORITY 1.0f

#define RENDERER_SAFECALL(func, message) Renderer::safeCallResult = func; if(Renderer::safeCallResult) { throw std::runtime_error(message); }

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

#define MAX_FRAMES_IN_FLIGHT 2

struct Shader;

class Renderer
{
public:

	Renderer(GLFWwindow* window);

    ~Renderer();

	void RegisterShader(Shader* shader);

	void UnregisterShader(Shader* shader);

	void DrawFrame();

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

	// Check if the device supports all the necessary extensions for this renderer.
	bool CheckDeviceExtensionSupport(VkPhysicalDevice device);

	struct SwapChainDetails
	{
		VkSurfaceCapabilitiesKHR m_capabilities;
		DynamicArray<VkSurfaceFormatKHR> m_formats;
		DynamicArray<VkPresentModeKHR> m_presentModes;
	};

	// Get information about the swap chain.
	SwapChainDetails* GetSwapChainSupportDetails(VkPhysicalDevice device);

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

	// Creates the window surface displayed on the OS window.
	void CreateWindowSurface();

	// Create the swap chain used to present images to the window.
	void CreateSwapChain();

	// Create swapchain image views.
	void CreateSwapChainImageViews();

	// Create render pass.
	void CreateRenderPass();

	// Create rendering pipeline
	void CreateGraphicsPipeline();

	// Create framebuffer.
	void CreateFramebuffers();

	// Create command pool.
	void CreateCommandPool();

	// Create semaphores.
	void CreateSyncObjects();

	// -----------------------------------------------------------------------------------------------------
	// Swap chain queries

	VkSurfaceFormatKHR ChooseSwapSurfaceFormat(DynamicArray<VkSurfaceFormatKHR>& availableFormats);

	VkPresentModeKHR ChooseSwapPresentMode(DynamicArray<VkPresentModeKHR>& availablePresentModes);

	VkExtent2D ChooseSwapExtent(VkSurfaceCapabilitiesKHR capabilities);

	// -----------------------------------------------------------------------------------------------------

	GLFWwindow* m_window;

	unsigned int m_windowWidth;
	unsigned int m_windowHeight;

	// -----------------------------------------------------------------------------------------------------
	// Validation layers
	static const DynamicArray<const char*> m_validationLayers;
	static const bool m_enableValidationLayers;

	VkDebugUtilsMessengerEXT m_messenger;

	// -----------------------------------------------------------------------------------------------------
	// Device extensions
	static const DynamicArray<const char*> m_deviceExtensions;

	// -----------------------------------------------------------------------------------------------------
	// Vulkan API
	VkInstance m_instance;
	VkPhysicalDevice m_physDevice;
	VkDevice m_logicDevice;

	// Queue families.
	VkQueue m_graphicsQueue;
	VkQueue m_presentQueue;
	VkQueue m_computeQueue;

	int m_graphicsQueueFamilyIndex;
	int m_presentQueueFamilyIndex;
	int m_computeQueueFamilyIndex;

	// Window surface.
	VkSurfaceKHR m_windowSurface;

	// Swap chain and swap chain images & framebuffers.
	VkFormat m_swapChainImageFormat;
	VkExtent2D m_swapChainImageExtents;
	VkSwapchainKHR m_swapChain;
	DynamicArray<VkImage> m_swapChainImages;
	DynamicArray<VkImageView> m_swapChainImageViews;
	DynamicArray<VkFramebuffer> m_swapChainFramebuffers;

	// Commands
	VkCommandPool m_commandPool;
	DynamicArray<VkCommandBuffer> m_commandBuffers;

	// Rendering.
	VkRenderPass m_renderPass;
	VkPipelineLayout m_pipelineLayout;
	VkPipeline m_graphicsPipeline;

	DynamicArray<VkSemaphore> m_imageAvailableSemaphores;
	DynamicArray<VkSemaphore> m_renderFinishedSemaphores;
	DynamicArray<VkFence> m_inFlightFences;
	unsigned long long m_currentFrame;

	// Extensions.
	VkExtensionProperties* m_extensions;
	unsigned int m_extensionCount;

	// -----------------------------------------------------------------------------------------------------
	// Shaders

	struct ShaderRegister 
	{
		ShaderRegister();

		VkShaderModule m_vertModule;
		VkShaderModule m_fragModule;
		bool m_registered;
	};

	// Temp
	Shader* m_triangleShader;

	Table<ShaderRegister> m_shaderRegisters;
};

