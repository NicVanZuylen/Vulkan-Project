#pragma once
#define GLFW_INCLUDE_VULKAN
#include "glfw3.h"
#include <stdexcept>
#include <iostream>
#include <thread>

#include "RendererHelper.h"

#include "DynamicArray.h"
#include "Queue.h"
#include "Table.h"
#include "glm.hpp"

#define QUEUE_PRIORITY 1.0f

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

#define MAX_FRAMES_IN_FLIGHT 2
#define MAX_CONCURRENT_COPIES MAX_FRAMES_IN_FLIGHT

#define DYNAMIC_SUBPASS_INDEX 0
#define LIGHTING_SUBPASS_INDEX 1

class Scene;
class LightingManager;
class RenderObject;
class Texture;

struct Shader;

struct CopyRequest 
{
	VkBuffer m_srcBuffer;
	VkBuffer m_dstBuffer;
	VkBufferCopy m_copyRegion;
};

class Renderer
{
public:

	Renderer(GLFWwindow* window);

    ~Renderer();

	// Functionality

	void SetWindow(GLFWwindow* window, const unsigned int& nWidth, const unsigned int& nHeight);

	void ResizeWindow(const unsigned int& nWidth, const unsigned int& nHeight, bool bNewSurface = false);

	// Get the scene objects will be rendered in.
	Scene* GetScene();

	// Begin the main render pass.
	void Begin();

	// Submit a copy operation to the GPU.
	void SubmitCopyOperation(VkCommandBuffer commandBuffer);

	// End the main render pass.
	void End();

	// Wait for the graphics queue to be idle.
	void WaitGraphicsIdle();

	// Wait for the transfer queue to be idle.
	void WaitTransferIdle();

	// Find the optimal memory type for allocating buffer memory.
	unsigned int FindMemoryType(unsigned int typeFilter, VkMemoryPropertyFlags propertyFlags);

	// Create a buffer with the provided size, usage flags, memory property flags, buffer and memory handles.
	void CreateBuffer(const unsigned long long& size, const VkBufferUsageFlags& bufferUsage, VkMemoryPropertyFlags properties, VkBuffer& bufferHandle, VkDeviceMemory& bufferMemory);

	// Create an image with the specified width, height, format, tiling and usage flags.
	void CreateImage(VkImage& image, VkDeviceMemory& imageMemory, const uint32_t& nWidth, const uint32_t& nHeight, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage);

	// Create an image view for the specified image.
	void CreateImageView(const VkImage& image, VkImageView& view, VkFormat format, VkImageAspectFlags aspectFlags);

	struct TempCmdBuffer 
	{
		VkCommandBuffer m_handle;
		VkFence m_destroyFence;
	};

	// Create a temporary command buffer for a one-time operation.
	TempCmdBuffer CreateTempCommandBuffer();

	// Destroy a temporary command buffer.
	void UseAndDestroyTempCommandBuffer(TempCmdBuffer& buffer);

	// Getters
	VkDevice GetDevice();

	VkPhysicalDevice GetPhysDevice();

	VkCommandPool GetCommandPool();

	const unsigned int& FrameWidth() const;

	const unsigned int& FrameHeight() const;

	const unsigned int SwapChainImageCount() const;

	VkFormat SwapChainImageFormat();

	DynamicArray<VkImageView>& SwapChainImageViews();

private:

	struct SwapChainDetails
	{
		VkSurfaceCapabilitiesKHR m_capabilities;
		DynamicArray<VkSurfaceFormatKHR> m_formats;
		DynamicArray<VkPresentModeKHR> m_presentModes;
	};

	// -----------------------------------------------------------------------------------------------------
	// Structure templates

	// Command buffer begin infos
	static VkCommandBufferBeginInfo m_standardCmdBeginInfo;
	static VkCommandBufferBeginInfo m_renderSecondaryCmdBeginInfo;

	// -----------------------------------------------------------------------------------------------------
	// Initialization functions

	// Create Vulkan instance.
	inline void CreateVKInstance();

	// Check if the necessary validation layers are supported for debug.
	inline void CheckValidationLayerSupport();

	// Get GPU to be used for rendering.
	inline void GetPhysicalDevice();

	// Create logical device to interface with the physical device.
	inline void CreateLogicalDevice();

	// Creates the window surface displayed on the OS window.
	inline void CreateWindowSurface();

	// Create the swap chain used to present images to the window.
	inline void CreateSwapChain();

	// Create swapchain image views.
	inline void CreateSwapChainImageViews();

	// Create command pools.
	inline void CreateCommandPools();

	// Create semaphores & fences.
	inline void CreateSyncObjects();

	// -----------------------------------------------------------------------------------------------------
	// Swap chain queries

	VkSurfaceFormatKHR ChooseSwapSurfaceFormat(DynamicArray<VkSurfaceFormatKHR>& availableFormats);

	VkPresentModeKHR ChooseSwapPresentMode(DynamicArray<VkPresentModeKHR>& availablePresentModes);

	VkExtent2D ChooseSwapExtent(VkSurfaceCapabilitiesKHR capabilities);

	// -----------------------------------------------------------------------------------------------------
	// Window

	GLFWwindow* m_window;

	unsigned int m_nWindowWidth;
	unsigned int m_nWindowHeight;

	// -----------------------------------------------------------------------------------------------------
	// Validation layers

	static const DynamicArray<const char*> m_validationLayers;
	static const bool m_bEnableValidationLayers;

	VkDebugUtilsMessengerEXT m_messenger;

	// -----------------------------------------------------------------------------------------------------
	// Device extensions

	static const DynamicArray<const char*> m_deviceExtensions;

	// -----------------------------------------------------------------------------------------------------
	// Vulkan Instance & Devices

	VkInstance m_instance;
	VkPhysicalDevice m_physDevice;
	VkDevice m_logicDevice;

	// -----------------------------------------------------------------------------------------------------
	// Queue families.

	static const RendererHelper::EQueueFamilyFlags m_eDesiredQueueFamilies;

	VkQueue m_graphicsQueue;
	VkQueue m_presentQueue;
	VkQueue m_transferQueue;
	VkQueue m_computeQueue;

	int m_nGraphicsQueueFamilyIndex;
	int m_nPresentQueueFamilyIndex;
	int m_nTransferQueueFamilyIndex;
	int m_nComputeQueueFamilyIndex;

	// -----------------------------------------------------------------------------------------------------
	// Window surface.
	VkSurfaceKHR m_windowSurface;

	// -----------------------------------------------------------------------------------------------------
	// Swap chain and swap chain images & framebuffers.

	VkFormat m_swapChainImageFormat;
	VkExtent2D m_swapChainImageExtents;
	VkSwapchainKHR m_swapChain;
	DynamicArray<VkImage> m_swapChainImages;
	DynamicArray<VkImageView> m_swapChainImageViews;

	// -----------------------------------------------------------------------------------------------------
	// Commands

	VkCommandPool m_mainGraphicsCommandPool;

    // -------------------------------------------------------------------------------------------------
	// Rendering

	VkSemaphore m_imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
	VkFence m_inFlightFences[MAX_FRAMES_IN_FLIGHT];
	uint64_t m_nElapsedFrames;
	uint32_t m_nFrameIndex;
	uint32_t m_nPresentImageIndex;
	
	Scene* m_scene;

	// -------------------------------------------------------------------------------------------------
	// Misc
	bool m_bMinimized;

	// Extensions.
	VkExtensionProperties* m_extensions;
	unsigned int m_nExtensionCount;
};

