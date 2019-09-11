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
#define MAX_CONCURRENT_COPIES 16

#define DYNAMIC_SUBPASS_INDEX 0
#define POST_SUBPASS_INDEX 1

class MeshRenderer;
class Texture;

struct Shader;

struct MVPUniformBuffer 
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

struct CopyRequest 
{
	VkBuffer srcBuffer;
	VkBuffer dstBuffer;
	VkBufferCopy copyRegion;
};

class Renderer
{
public:

	Renderer(GLFWwindow* window);

    ~Renderer();

	// Functionality

	void SetWindow(GLFWwindow* window, const unsigned int& nWidth, const unsigned int& nHeight);

	void ResizeWindow(const unsigned int& nWidth, const unsigned int& nHeight, bool bNewSurface = false);

	// Begin the main render pass.
	void Begin();

	// Submit a copy operation to the GPU.
	void SubmitCopyOperation(VkCommandBuffer commandBuffer);

	// Request a dedicated transfer operation.
	void RequestCopy(const CopyRequest& request);

	// Force re-recording of the dynamic rendering commands.
	void ForceDynamicStateChange();

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

	VkRenderPass DynamicRenderPass();

	VkDescriptorSetLayout MVPUBOSetLayout();

	VkBuffer MVPUBOHandle(const unsigned int& nSwapChainImageIndex);

	const DynamicArray<VkFramebuffer>& GetFramebuffers() const;

	const unsigned int& FrameWidth() const;

	const unsigned int& FrameHeight() const;

	const unsigned int SwapChainImageCount() const;

	// Setters
	void SetViewMatrix(glm::mat4& viewMat);

private:

	struct SwapChainDetails
	{
		VkSurfaceCapabilitiesKHR m_capabilities;
		DynamicArray<VkSurfaceFormatKHR> m_formats;
		DynamicArray<VkPresentModeKHR> m_presentModes;
	};

	static DynamicArray<CopyRequest> m_newCopyRequests;

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

	// Create basic framebuffers.
	inline void CreateFramebufferImages();

	// Destroy basic framebuffers.
	inline void DestroyFramebufferImages();

	// Create render pass.
	inline void CreateRenderPasses();

	// Create framebuffer.
	inline void CreateFramebuffers();

	// Create MVP descriptor set layout.
	inline void CreateMVPDescriptorSetLayout();

	// Deferred lighting set layout.
	inline void CreateLightingDescriptorSetLayout();

	// Create MVP uniform buffers.
	inline void CreateMVPUniformBuffers();

	// Create global descriptor pool, global descriptors are allocated from it.
	inline void CreateDescriptorPool();

	// Create UBO MVP descriptor sets.
	inline void CreateUBOMVPDescriptorSets();

	// Create Deferred lighting pass descriptor set.
	inline void CreateLightingDescriptorSet(bool bAllocNew = true);

	// Create Deferred Shading pass pipeline.
	inline void CreateLightingPipeline();

	// Create command pools.
	inline void CreateCommandPools();

	// Create main command buffers.
	inline void CreateCmdBuffers();

	// Create semaphores & fences.
	inline void CreateSyncObjects();

    // Update MVP Uniform buffer contents associated with the provided swap chain image.
	inline void UpdateMVP(const unsigned int& bufferIndex);

	// Record transfer command buffer.
	inline void RecordTransferCommandBuffer(const unsigned int& bufferIndex);

	// Record main primary command buffer.
	inline void RecordMainCommandBuffer(const unsigned int& bufferIndex);

	// Record dynamic secondary command buffer.
	inline void RecordDynamicCommandBuffer(const unsigned int& bufferIndex, const unsigned int& frameIndex);

	// Record post-pass secondary command buffers.
	inline void RecordPostPassCommandBuffers();

	// Submit transfer operations to the GPU.
	void SubmitTransferOperations();

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
	// Vulkan API
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
	DynamicArray<VkFramebuffer> m_swapChainFramebuffers;

	// -----------------------------------------------------------------------------------------------------
	// Images and framebuffers.
	Texture* m_depthImage;

	// G-Buffers
	Texture* m_colorImage;
	Texture* m_posImage;
	Texture* m_normalImage;

	// -----------------------------------------------------------------------------------------------------
	// Commands
	VkCommandPool m_mainGraphicsCommandPool;
	VkCommandPool m_postPassGraphicsCmdPool;
	VkCommandPool m_transferCmdPool;
	DynamicArray<VkCommandBuffer> m_mainPrimaryCmdBufs;
	DynamicArray<VkCommandBuffer> m_dynamicPassCmdBufs; // Command buffers for the dynamic render pass.
	DynamicArray<VkCommandBuffer> m_postPassCmdBufs; // Post-pass command buffers.

	// Transfer queue submission info moved here to move it from the stack, where it would remain for as long as the thread lives.
	VkSubmitInfo m_transSubmitInfo;
	DynamicArray<VkCommandBuffer> m_transferCmdBufs; // Command buffer for dedicated transfer operations.

	// -----------------------------------------------------------------------------------------------------
	// Descriptors
	VkDescriptorSetLayout m_uboDescriptorSetLayout;
	VkDescriptorSetLayout m_lightingPassSetLayout;

	VkDescriptorPool m_descriptorPool;
	DynamicArray<VkDescriptorSet> m_uboDescriptorSets;
	VkDescriptorSet m_lightingDescriptorSet;

	// -----------------------------------------------------------------------------------------------------
	// Descriptor buffers
	DynamicArray<VkBuffer> m_mvpBuffers;
	DynamicArray<VkDeviceMemory> m_mvpBufferMemBlocks;

	MVPUniformBuffer m_mvp;

	// -----------------------------------------------------------------------------------------------------
	// Graphics Pipelines

	VkPipelineLayout m_lightingPipelineLayout;
	VkPipeline m_lightingPassPipeline;

	// -----------------------------------------------------------------------------------------------------
	// Lighting

	Shader* m_lightingPassShader;

	// -----------------------------------------------------------------------------------------------------
	// Rendering

	VkRenderPass m_mainRenderPass;

	DynamicArray<MeshRenderer*> m_dynamicObjects;
	bool m_dynamicStateChange[3];

	DynamicArray<VkSemaphore> m_imageAvailableSemaphores;
	DynamicArray<VkSemaphore> m_renderFinishedSemaphores;
	DynamicArray<VkFence> m_copyReadyFences;
	DynamicArray<VkFence> m_inFlightFences;
	unsigned long long m_nCurrentFrame;
	unsigned int m_nCurrentFrameIndex;
	unsigned int m_nPresentImageIndex;
	
	DynamicArray<DynamicArray<CopyRequest>> m_transferBuffers; // Arrays of transfer request buffer arrays for each concurrent transfer command buffer.
	Queue<unsigned int> m_transferIndices;
	std::thread* m_transferThread;
	unsigned int m_nTransferFrameIndex;
	bool m_bTransferThread;
	bool m_bTransferReady;

	// Misc
	bool m_bMinimized;

	// Extensions.
	VkExtensionProperties* m_extensions;
	unsigned int m_extensionCount;
};

