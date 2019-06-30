#pragma once
#define GLFW_INCLUDE_VULKAN
#include "glfw3.h"
#include <stdexcept>
#include <iostream>

#include "DynamicArray.h"
#include "Table.h"
#include "glm.hpp"

#define QUEUE_PRIORITY 1.0f

#ifdef RENDERER_DEBUG

#define RENDERER_SAFECALL(func, message) Renderer::m_safeCallResult = func; if(Renderer::m_safeCallResult) { throw std::runtime_error(message); }

#else

#define RENDERER_SAFECALL(func, message) Renderer::m_safeCallResult = func; //message

#endif

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

#define MAX_FRAMES_IN_FLIGHT 2

class MeshRenderer;

struct Shader;

struct MVPUniformBuffer 
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

class Renderer
{
public:

	Renderer(GLFWwindow* window);

    ~Renderer();

	// Functionality
	void RegisterShader(Shader* shader);

	void UnregisterShader(Shader* shader);

	// Begin the main render pass.
	void Begin();

	// Submit a copy operation to the GPU.
	void SubmitCopyOperation(VkCommandBuffer commandBuffer);

	// Schedule a render object to be drawn.
	void AddDynamicObject(MeshRenderer* object);

	// End the main render pass.
	void End();

	// Wait for the graphics queue to be idle.
	void WaitGraphicsIdle();

	// Create a buffer with the provided size, usage flags, memory property flags, buffer and memory handles.
	void CreateBuffer(const unsigned long long& size, const VkBufferUsageFlags& bufferUsage, VkMemoryPropertyFlags properties, VkBuffer& bufferHandle, VkDeviceMemory& bufferMemory);

	// Getters
	VkDevice GetDevice();

	VkPhysicalDevice GetPhysDevice();

	VkCommandPool GetCommandPool();

	VkRenderPass DynamicRenderPass();

	VkDescriptorSetLayout MVPUBOSetLayout();

	const DynamicArray<VkFramebuffer>& GetFramebuffers();

	unsigned int FrameWidth();

	unsigned int FrameHeight();

	// Setters
	void SetViewMatrix(glm::mat4& viewMat);

	// Debug
	static VkResult m_safeCallResult;

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
	void CreateRenderPasses();

	// Create MVP descriptor set layout.
	void CreateMVPDescriptorSetLayout();

	// Create MVP uniform buffers.
	void CreateMVPUniformBuffers();

	// Create UBO MVP descriptor pool.
	void CreateUBOMVPDescriptorPool();

	// Create UBO MVP descriptor sets.
	void CreateUBOMVPDescriptorSets();

	// Create framebuffer.
	void CreateFramebuffers();

	// Create command pool.
	void CreateCommandPool();

    // Update MVP Uniform buffer contents associated with the provided swap chain image.
	void UpdateMVP(const unsigned int& bufferIndex);

	// Record command buffer.
	void RecordDynamicCommandBuffer(const unsigned int& bufferIndex);

	// Create semaphores.
	void CreateSyncObjects();

    // Find the optimal memory type for allocating buffer memory.
	unsigned int FindMemoryType(unsigned int typeFilter, VkMemoryPropertyFlags propertyFlags);

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
	DynamicArray<VkCommandBuffer> m_staticPassBufs; // Command buffers for the static render pass.
	DynamicArray<VkCommandBuffer> m_dynamicPassBufs; // Command buffers for the dynamic render pass.

	// Descriptors
	VkDescriptorSetLayout m_uboDescriptorSetLayout;

	VkDescriptorPool m_uboDescriptorPool;
	DynamicArray<VkDescriptorSet> m_uboDescriptorSets;

	// Descriptor buffers
	DynamicArray<VkBuffer> m_mvpBuffers;
	DynamicArray<VkDeviceMemory> m_mvpBufferMemBlocks;

	MVPUniformBuffer m_mvp;

	// Rendering
	VkRenderPass m_staticRenderPass;
	VkRenderPass m_dynamicRenderPass;

	DynamicArray<MeshRenderer*> m_dynamicObjects;
	bool m_dynamicStateChange[3];

	DynamicArray<VkSemaphore> m_imageAvailableSemaphores;
	DynamicArray<VkSemaphore> m_renderFinishedSemaphores;
	DynamicArray<VkFence> m_inFlightFences;
	unsigned long long m_currentFrame;
	unsigned int m_currentFrameIndex;
	unsigned int m_presentImageIndex;

	// Extensions.
	VkExtensionProperties* m_extensions;
	unsigned int m_extensionCount;
};

