#include "Renderer.h"
#include <set>
#include <thread>

#include "LightingManager.h"
#include "Shader.h"
#include "Material.h"
#include "RenderObject.h"
#include "Texture.h"
#include "gtc/matrix_transform.hpp"

#include "SubScene.h"

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

const RendererHelper::EQueueFamilyFlags Renderer::m_eDesiredQueueFamilies = static_cast<RendererHelper::EQueueFamilyFlags>
(
    RendererHelper::EQueueFamilyFlags::QUEUE_FAMILY_PRESENT |
    RendererHelper::EQueueFamilyFlags::QUEUE_FAMILY_GRAPHICS |
    RendererHelper::EQueueFamilyFlags::QUEUE_FAMILY_COMPUTE |
    RendererHelper::EQueueFamilyFlags::QUEUE_FAMILY_TRANSFER
);

// Template structures

VkCommandBufferBeginInfo Renderer::m_standardCmdBeginInfo =
{
	VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
	nullptr,
	VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
	nullptr
};

VkCommandBufferBeginInfo Renderer::m_renderSecondaryCmdBeginInfo =
{
	VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
	nullptr,
	VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
	nullptr
};

#ifndef RENDERER_DEBUG

const bool Renderer::m_bEnableValidationLayers = false;

#else

const bool Renderer::m_bEnableValidationLayers = true;

#endif

Renderer::Renderer(GLFWwindow* window)
{
	m_window = window;
	m_extensions = nullptr;
	m_nExtensionCount = 0;

	m_nWindowWidth = WINDOW_WIDTH;
	m_nWindowHeight = WINDOW_HEIGHT;

	m_instance = VK_NULL_HANDLE;
	m_physDevice = VK_NULL_HANDLE;

	m_nGraphicsQueueFamilyIndex = -1;
	m_nPresentQueueFamilyIndex = -1;
	m_nTransferQueueFamilyIndex = -1;
	m_nComputeQueueFamilyIndex = -1;

	m_bMinimized = false;

	// Check for validation layer support.
	CheckValidationLayerSupport();

	// Vulkan instance
	CreateVKInstance();

	// Debug
	if (m_bEnableValidationLayers) 
	{
		RendererHelper::SetupDebugMessenger(m_instance, m_messenger);
	}

	// Window
	CreateWindowSurface();

	// Phyiscal device
	GetPhysicalDevice();

	// Device & command pools
	CreateLogicalDevice();
	CreateCommandPools();

	// Get queue handles...
	vkGetDeviceQueue(m_logicDevice, m_nPresentQueueFamilyIndex, 0, &m_presentQueue);
	vkGetDeviceQueue(m_logicDevice, m_nGraphicsQueueFamilyIndex, 0, &m_graphicsQueue);
	vkGetDeviceQueue(m_logicDevice, m_nTransferQueueFamilyIndex, 0, &m_transferQueue);
	vkGetDeviceQueue(m_logicDevice, m_nComputeQueueFamilyIndex, 0, &m_computeQueue); // May also be the graphics queue.

	// Swap Chain Images
	CreateSwapChain();
	CreateSwapChainImageViews();

	EGBufferAttachmentTypeBit gBufferBits = (EGBufferAttachmentTypeBit)(GBUFFER_COLOR_BIT | GBUFFER_COLOR_HDR_BIT | GBUFFER_DEPTH_BIT | GBUFFER_POSITION_BIT | GBUFFER_NORMAL_BIT);

	m_scene = new Scene(this, m_nWindowWidth, m_nWindowHeight, m_nGraphicsQueueFamilyIndex);

	// Syncronization
	CreateSyncObjects();

	WaitGraphicsIdle();
	WaitTransferIdle();
}

Renderer::~Renderer()
{
	vkDeviceWaitIdle(m_logicDevice);

	// Delete scene.
	delete m_scene;

	// Destroy sync objects.
	for(uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		vkDestroyFence(m_logicDevice, m_inFlightFences[i], nullptr);

	    vkDestroySemaphore(m_logicDevice, m_imageAvailableSemaphores[i], nullptr);
	}

	vkDeviceWaitIdle(m_logicDevice);

	// Destroy command pools.
	vkDestroyCommandPool(m_logicDevice, m_mainGraphicsCommandPool, nullptr);

	delete[] m_extensions;
	m_extensions = nullptr;

	// Destroy debug messenger.
	if(m_bEnableValidationLayers)
	    RendererHelper::DestroyDebugUtilsMessengerEXT(m_instance, nullptr, &m_messenger);

	// Destroy image views.
	for (uint32_t i = 0; i < m_swapChainImageViews.Count(); ++i)
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

void Renderer::SetWindow(GLFWwindow* window, const unsigned int& nWidth, const unsigned int& nHeight) 
{
	// Set new window.
	m_window = window;

	// Resize window and flag a surface recreation.
	ResizeWindow(nWidth, nHeight, true);

	m_nElapsedFrames = 0;
	m_nFrameIndex = 0;
	m_nPresentImageIndex = 0;
}

void Renderer::ResizeWindow(const unsigned int& nWidth, const unsigned int& nHeight, bool bNewSurface) 
{
	// Set new desired dimensions.
	m_nWindowWidth = nWidth;
	m_nWindowHeight = nHeight;

	// Window will not be resized if it is set to zero width or height, (minimization causes this)

	if (!nWidth || !nHeight) 
	{
		m_bMinimized = true;
		return;
	}

	m_bMinimized = false;

	// ---------------------------------------------------------------------------------------------------------
	// Deletion

	// Wait for device to idle.
	vkDeviceWaitIdle(m_logicDevice);

	// Destroy swap chain image views.
	for (uint32_t i = 0; i < m_swapChainImageViews.Count(); ++i)
	{
		vkDestroyImageView(m_logicDevice, m_swapChainImageViews[i], nullptr);
		m_swapChainImageViews[i] = nullptr;
	}

	// Destroy swap chain.
	vkDestroySwapchainKHR(m_logicDevice, m_swapChain, nullptr);
	m_swapChain = nullptr;

	if(bNewSurface) 
	{
		// Destroy window surface.
		vkDestroySurfaceKHR(m_instance, m_windowSurface, nullptr);
	}

	// ---------------------------------------------------------------------------------------------------------
	// Recreation

	if(bNewSurface) 
	{
		// Create new window surface.
		CreateWindowSurface();

		// Check for surface present suppport.
		VkBool32 hasPresentSupport = VK_FALSE;
	    RENDERER_SAFECALL(vkGetPhysicalDeviceSurfaceSupportKHR(m_physDevice, m_nPresentQueueFamilyIndex, m_windowSurface, &hasPresentSupport), "Renderer Error: Failed to get surface support confirmation on new window surface.");

		if (!hasPresentSupport)
			throw std::runtime_error("Renderer Error: Fullscreen window does not support new surface.");
	}

	// Recreate swap chain and swap chain images.
	CreateSwapChain();
	CreateSwapChainImageViews();

	// ---------------------------------------------------------------------------------------------------------
	// Semaphore recreation.

	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	// Re-create all rendering and transfer semaphores.
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		vkDestroySemaphore(m_logicDevice, m_imageAvailableSemaphores[i], nullptr);

		RENDERER_SAFECALL(vkCreateSemaphore(m_logicDevice, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]), "Renderer Error: Failed to create semaphores.");
	}

	// Reset frame count and indices.
	m_nElapsedFrames = 0;
	m_nFrameIndex = 1;

	// Re-create subscenes.
	m_scene->ResizeOutput(m_nWindowWidth, m_nWindowHeight);
}

Scene* Renderer::GetScene()
{
	return m_scene;
}

void Renderer::CreateVKInstance() 
{
	// ----------------------------------------------------------------------------------------------------------
	// Extension count.

	VkResult result = vkEnumerateInstanceExtensionProperties(nullptr, &m_nExtensionCount, nullptr);

	m_extensions = new VkExtensionProperties[m_nExtensionCount];

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

	result = vkEnumerateInstanceExtensionProperties(nullptr, &m_nExtensionCount, m_extensions);

	std::cout << "Renderer Info: Available extensions are: \n" << std::endl;

	for (unsigned int i = 0; i < m_nExtensionCount; ++i)
		std::cout << m_extensions[i].extensionName << "\n";

	std::cout << "\n";
}

void Renderer::CheckValidationLayerSupport() 
{
	if (!m_bEnableValidationLayers)
		return;

	// Get available validation layers...
	unsigned int layerCount = 0;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	DynamicArray<VkLayerProperties> availableLayers(layerCount, 1);
	availableLayers.SetCount(layerCount);

	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.Data());

	// Compare against required layers...
	uint32_t successCount = 0;
	for(uint32_t i = 0; i < m_validationLayers.Count(); ++i)
	{
		for (uint32_t j = 0; j < availableLayers.Count(); ++j)
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

void Renderer::GetPhysicalDevice() 
{
	unsigned int nDeviceCount = 0;
	RENDERER_SAFECALL(vkEnumeratePhysicalDevices(m_instance, &nDeviceCount, nullptr), "Renderer Error: Failed to get physical device count.");

	DynamicArray<VkPhysicalDevice> devices(nDeviceCount, 1);
	devices.SetCount(nDeviceCount);

	RENDERER_SAFECALL(vkEnumeratePhysicalDevices(m_instance, &nDeviceCount, devices.Data()), "Renderer Error: Failed to obtain physical device data.");

	// Check if devices are suitable and the first suitable device.
	int bestScore = 0;
	for(unsigned int i = 0; i < nDeviceCount; ++i)
	{
		int suitabilityScore = RendererHelper::DeviceSuitable(m_windowSurface, devices[i], m_deviceExtensions, m_eDesiredQueueFamilies);
		if (suitabilityScore > bestScore) 
		{
			m_physDevice = devices[i];
		}
	}

	// Get queue family indices.
	RendererHelper::QueueFamilyIndices indices = RendererHelper::FindQueueFamilies(m_windowSurface, m_physDevice, m_eDesiredQueueFamilies);

	m_nPresentQueueFamilyIndex = indices.m_nPresentFamilyIndex;
	m_nGraphicsQueueFamilyIndex = indices.m_nGraphicsFamilyIndex;
	m_nComputeQueueFamilyIndex = indices.m_nComputeFamilyIndex;
	m_nTransferQueueFamilyIndex = indices.m_nTransferFamilyIndex;

	if (m_physDevice == VK_NULL_HANDLE)
		throw std::runtime_error("Renderer Error: Failed to find suitable GPU!");
}

void Renderer::CreateLogicalDevice()
{
	DynamicArray<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<int> uniqueQueueFamilies = { m_nPresentQueueFamilyIndex, m_nGraphicsQueueFamilyIndex, m_nComputeQueueFamilyIndex, m_nTransferQueueFamilyIndex };

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
	if (m_bEnableValidationLayers)
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
	RendererHelper::SwapChainDetails* details = RendererHelper::GetSwapChainSupportDetails(m_windowSurface, m_physDevice);

	// Find best format, present mode, and image extents for the swap chain images.
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
		static_cast<unsigned int>(m_nPresentQueueFamilyIndex), 
		static_cast<unsigned int>(m_nGraphicsQueueFamilyIndex) 
	};

	if(m_nGraphicsQueueFamilyIndex != m_nPresentQueueFamilyIndex) 
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

	RENDERER_SAFECALL(vkCreateSwapchainKHR(m_logicDevice, &createInfo, nullptr, &m_swapChain), "Renderer Error: Failed to create swapchain!");

	// Retrieve swap chain image count.
	unsigned int swapChainImageCount = 0;
	RENDERER_SAFECALL(vkGetSwapchainImagesKHR(m_logicDevice, m_swapChain, &swapChainImageCount, nullptr), "Renderer Error: Failed to retrieve swap chain image count.");

	// Resize handle array to match image count.
	m_swapChainImages.SetSize(swapChainImageCount);
	m_swapChainImages.SetCount(swapChainImageCount);

	// Retrieve actual swap chain images.
	RENDERER_SAFECALL(vkGetSwapchainImagesKHR(m_logicDevice, m_swapChain, &swapChainImageCount, m_swapChainImages.Data()), "Renderer Error: Failed to retieve swap chain images.");

	delete details;
}

void Renderer::CreateSwapChainImageViews() 
{
	m_swapChainImageViews.SetSize(m_swapChainImages.GetSize());
	m_swapChainImageViews.SetCount(m_swapChainImageViews.GetSize());

	for (uint32_t i = 0; i < m_swapChainImages.Count(); ++i)
	{
		VkImageView view = nullptr;
		VkImageViewCreateInfo viewCreateInfo = {};

		viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCreateInfo.image = m_swapChainImages[i];
		viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewCreateInfo.format = m_swapChainImageFormat;
		viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY; // Components are independent.
		viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

		viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // Swap chain images are of course color buffers.
		viewCreateInfo.subresourceRange.baseMipLevel = 0;
		viewCreateInfo.subresourceRange.levelCount = 1;
		viewCreateInfo.subresourceRange.baseArrayLayer = 0;
		viewCreateInfo.subresourceRange.layerCount = 1;

		RENDERER_SAFECALL(vkCreateImageView(m_logicDevice, &viewCreateInfo, nullptr, &view), "Renderer Error: Failed to create swap chain image views.");

		m_swapChainImageViews[i] = view;
	}
}

void Renderer::CreateCommandPools() 
{
	// ==================================================================================================================================================
	// Graphics command pool.

	VkCommandPoolCreateInfo poolCreateInfo = {};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolCreateInfo.queueFamilyIndex = m_nGraphicsQueueFamilyIndex;
	poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

	RENDERER_SAFECALL(vkCreateCommandPool(m_logicDevice, &poolCreateInfo, nullptr, &m_mainGraphicsCommandPool), "Renderer Error: Failed to create main graphics command pool.");
}

void Renderer::CreateSyncObjects()
{
	// Sempahores and fences are needed for each potential frame in flight.
	VkSemaphoreCreateInfo semaphoreInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, nullptr, 0 };

	// Frame-in-flight fences will start signaled.
	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	// Create rendering, present and transfer semaphores. And frame-in-flight fences.
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		RENDERER_SAFECALL(vkCreateSemaphore(m_logicDevice, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]), "Renderer Error: Failed to create semaphores.");

		RENDERER_SAFECALL(vkCreateFence(m_logicDevice, &fenceInfo, nullptr, &m_inFlightFences[i]), "Renderer Error: Failed to create in-flight fence.");
	}
}

void Renderer::Begin() 
{
	// Do not attempt to render to a zero sized window.
	if (m_bMinimized)
		return;

	// ----------------------------------------------------------------------------------------------
	// The frame in flight index will constantly loop around as new frames in flight are created and finished.
	m_nFrameIndex = m_nElapsedFrames++ % MAX_FRAMES_IN_FLIGHT;

	// ----------------------------------------------------------------------------------------------
	// Wait for frames-in-flight.
	vkWaitForFences(m_logicDevice, 1, &m_inFlightFences[m_nFrameIndex], VK_TRUE, ~(0ULL));
	vkResetFences(m_logicDevice, 1, &m_inFlightFences[m_nFrameIndex]);

	// ----------------------------------------------------------------------------------------------
	// Aquire next image to render to from the swap chain.

	RENDERER_SAFECALL(vkAcquireNextImageKHR(m_logicDevice, m_swapChain, ~(0ULL), m_imageAvailableSemaphores[m_nFrameIndex], VK_NULL_HANDLE, &m_nPresentImageIndex),
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

void Renderer::End()
{
	// Do not attempt to render to a zero sized window.
	if (m_bMinimized)
		return;

	// ----------------------------------------------------------------------------------------------
	// Submit scene rendering commands
	
	VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;

	m_scene->DrawSubscenes(m_nPresentImageIndex, m_nElapsedFrames, m_nFrameIndex, m_imageAvailableSemaphores[m_nFrameIndex], renderFinishedSemaphore, m_inFlightFences[m_nFrameIndex]);

	// ----------------------------------------------------------------------------------------------
	// Submit rendered frame to the swap chain for presentation.

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderFinishedSemaphore; // Wait for render to finish before presenting.
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &m_swapChain;
	presentInfo.pImageIndices = &m_nPresentImageIndex;
	presentInfo.pResults = nullptr;

	VkResult result = vkQueuePresentKHR(m_graphicsQueue, &presentInfo);

	// ----------------------------------------------------------------------------------------------
	// Ensure window surface & graphics pipelines meet presentation requirements.

	if (result == VK_ERROR_OUT_OF_DATE_KHR) 
	{
		ResizeWindow(m_nWindowWidth, m_nWindowHeight, true);
	}
	else if(result)
		throw std::runtime_error("Renderer Error: Failed to present swap chain image.");

	// ----------------------------------------------------------------------------------------------
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

void Renderer::CreateImage(VkImage& image, VkDeviceMemory& imageMemory, const uint32_t& nWidth, const uint32_t& nHeight, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage)
{
	VkImageCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	createInfo.imageType = VK_IMAGE_TYPE_2D;
	createInfo.extent.width = nWidth;
	createInfo.extent.height = nHeight;
	createInfo.extent.depth = 1;
	createInfo.mipLevels = 1;
	createInfo.arrayLayers = 1;
	createInfo.format = format;
	createInfo.tiling = tiling;
	createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	createInfo.usage = usage;
	createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	createInfo.flags = 0;

	// Create image.
	RENDERER_SAFECALL(vkCreateImage(m_logicDevice, &createInfo, nullptr, &image), "Renderer Error: Failed to create image object.");

	VkMemoryRequirements imageMemRequirements;
	vkGetImageMemoryRequirements(m_logicDevice, image, &imageMemRequirements);

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = imageMemRequirements.size;
	allocInfo.memoryTypeIndex = FindMemoryType(imageMemRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	RENDERER_SAFECALL(vkAllocateMemory(m_logicDevice, &allocInfo, nullptr, &imageMemory), "Renderer Error: Failed to allocate texture image memory.");

	// Bind image memory to the image.
	vkBindImageMemory(m_logicDevice, image, imageMemory, 0);
}

void Renderer::CreateImageView(const VkImage& image, VkImageView& view, VkFormat format, VkImageAspectFlags aspectFlags) 
{
	VkImageViewCreateInfo viewCreateInfo = {};
	viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCreateInfo.image = image;
	viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewCreateInfo.format = format;
	viewCreateInfo.subresourceRange.aspectMask = aspectFlags;
	viewCreateInfo.subresourceRange.baseMipLevel = 0;
	viewCreateInfo.subresourceRange.levelCount = 1;
	viewCreateInfo.subresourceRange.baseArrayLayer = 0;
	viewCreateInfo.subresourceRange.layerCount = 1;

	RENDERER_SAFECALL(vkCreateImageView(m_logicDevice, &viewCreateInfo, nullptr, &view), "Renderer Error: Failed to create image view.");
}

Renderer::TempCmdBuffer Renderer::CreateTempCommandBuffer()
{
	TempCmdBuffer tempBuffer;

	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = m_mainGraphicsCommandPool;
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
	vkFreeCommandBuffers(m_logicDevice, m_mainGraphicsCommandPool, 1, &buffer.m_handle);
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
	return m_mainGraphicsCommandPool;
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

VkFormat Renderer::SwapChainImageFormat()
{
	return m_swapChainImageFormat;
}

DynamicArray<VkImageView>& Renderer::SwapChainImageViews()
{
	return m_swapChainImageViews;
}

VkSurfaceFormatKHR Renderer::ChooseSwapSurfaceFormat(DynamicArray<VkSurfaceFormatKHR>& availableFormats) 
{
	VkSurfaceFormatKHR desiredFormat = { VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };

	if (availableFormats.Count() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED) // Can use any format.
	{
		return desiredFormat; // So use the desired format.
	}

	for (uint32_t i = 0; i < availableFormats.Count(); ++i)
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

	for (uint32_t i = 0; i < availablePresentModes.Count(); ++i)
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

	VkExtent2D desiredExtent = { m_nWindowWidth, m_nWindowHeight };

	// Clamp to usable range.
	desiredExtent.width = glm::clamp(desiredExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
	desiredExtent.height = glm::clamp(desiredExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

	return desiredExtent;
}