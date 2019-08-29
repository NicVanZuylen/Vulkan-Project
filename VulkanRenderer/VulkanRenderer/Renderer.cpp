#include "Renderer.h"
#include <algorithm>
#include <set>
#include <thread>

#include "Shader.h"
#include "Material.h"
#include "MeshRenderer.h"
#include "Texture.h"
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

const RendererHelper::EQueueFamilyFlags Renderer::m_eDesiredQueueFamilies = static_cast<RendererHelper::EQueueFamilyFlags>
(
    RendererHelper::EQueueFamilyFlags::QUEUE_FAMILY_PRESENT |
    RendererHelper::EQueueFamilyFlags::QUEUE_FAMILY_GRAPHICS |
    RendererHelper::EQueueFamilyFlags::QUEUE_FAMILY_COMPUTE |
    RendererHelper::EQueueFamilyFlags::QUEUE_FAMILY_TRANSFER
);

#ifndef RENDERER_DEBUG

const bool Renderer::m_bEnableValidationLayers = false;

#else

const bool Renderer::m_bEnableValidationLayers = true;

#endif

VkResult Renderer::m_safeCallResult = VK_SUCCESS;
DynamicArray<CopyRequest> Renderer::m_newTransferRequests;

Renderer::Renderer(GLFWwindow* window)
{
	m_window = window;
	m_extensions = nullptr;
	m_extensionCount = 0;

	m_windowWidth = WINDOW_WIDTH;
	m_windowHeight = WINDOW_HEIGHT;

	m_instance = VK_NULL_HANDLE;
	m_physDevice = VK_NULL_HANDLE;

	m_nGraphicsQueueFamilyIndex = -1;
	m_nPresentQueueFamilyIndex = -1;
	m_nTransferQueueFamilyIndex = -1;
	m_nComputeQueueFamilyIndex = -1;

	m_transferBuffers.SetSize(MAX_CONCURRENT_COPIES);
	m_transferBuffers.SetCount(MAX_CONCURRENT_COPIES);
	m_bTransferThread = true;
	m_bTransferReady = true;
	m_transferThread = new std::thread(&Renderer::SubmitTransferOperations, this);

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
	CreateCommandPool();

	// Get queue handles...
	vkGetDeviceQueue(m_logicDevice, m_nPresentQueueFamilyIndex, 0, &m_presentQueue);
	vkGetDeviceQueue(m_logicDevice, m_nGraphicsQueueFamilyIndex, 0, &m_graphicsQueue);
	vkGetDeviceQueue(m_logicDevice, m_nTransferQueueFamilyIndex, 0, &m_transferQueue);
	vkGetDeviceQueue(m_logicDevice, m_nComputeQueueFamilyIndex, 0, &m_computeQueue); // May also be the graphics queue.

	// Images
	CreateSwapChain();
	CreateSwapChainImageViews();
	CreateDepthImage();

	// Render passes
	CreateRenderPasses();

	// Buffers & descriptors setup
	CreateMVPDescriptorSetLayout();
	CreateMVPUniformBuffers();
	CreateUBOMVPDescriptorPool();
	CreateUBOMVPDescriptorSets();

	// Framebuffers & main command buffers
	CreateFramebuffers();
	CreateCmdBuffers();

	// Syncronization
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
	vkDestroyRenderPass(m_logicDevice, m_mainRenderPass, nullptr);

	delete[] m_extensions;

	// Destroy debug messenger.
	if(m_bEnableValidationLayers)
	    RendererHelper::DestroyDebugUtilsMessengerEXT(m_instance, nullptr, &m_messenger);

	delete m_depthImage;

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

	vkCreateSwapchainKHR(m_logicDevice, &createInfo, nullptr, &m_swapChain);

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

	for (int i = 0; i < m_swapChainImages.Count(); ++i) 
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

void Renderer::CreateDepthImage() 
{
	// Specify pool of depth formats and find the best available format.
	DynamicArray<VkFormat> formats = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
	VkFormat depthFormat = RendererHelper::FindBestDepthFormat(m_physDevice, formats, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

	// Create the depth buffer image.
	m_depthImage = new Texture(this, m_swapChainImageExtents.width, m_swapChainImageExtents.height, ATTACHMENT_DEPTH_STENCIL, depthFormat);
}

void Renderer::CreateRenderPasses() 
{
	// ------------------------------------------------------------------------------------------------------------
	// Dynamic object pass

	{
		VkAttachmentDescription colorAttachment = {};
		colorAttachment.format = m_swapChainImageFormat;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT; // Not multisampling right now.
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // Clear on load.
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // Store color attachment for presentation.
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // Don't care.
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // Don't care.
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Layout could be undefined or VK_IMAGE_LAYOUT_PRESENT_SRC_KHR.
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // Output layout should be presentable to the screen.

		VkAttachmentReference colorAttachmentRef = {};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // Expected optimal layout.

		VkAttachmentDescription depthAttachment = {};
		depthAttachment.format = m_depthImage->Format();
		depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT; // No multisampling.
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // Load from previous pass.
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // Store.
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // We are not using stencil for now, so we don't care.
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // Don't care.
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Layout could be undefined or VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL.
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthAttachmentRef = {};
		depthAttachmentRef.attachment = 1;
		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; // Expected optimal depth/stencil layout.

		VkAttachmentReference colorAttachmentRefs[] = { colorAttachmentRef };

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1; // One attachment.
		subpass.pColorAttachments = colorAttachmentRefs;
		subpass.pDepthStencilAttachment = &depthAttachmentRef;

		VkAttachmentDescription attachments[] = { colorAttachment, depthAttachment };

		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 2;
		renderPassInfo.pAttachments = attachments;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;

		VkSubpassDependency dependency = {};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT; // Ensures color & depth/stencil output has completed.
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
			| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		RENDERER_SAFECALL(vkCreateRenderPass(m_logicDevice, &renderPassInfo, nullptr, &m_mainRenderPass), "Renderer Error: Failed to create render pass.");
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
	m_mvpBuffers.SetSize(MAX_FRAMES_IN_FLIGHT); // One buffer needs to exist for each "Frame in flight"/frame waiting on the queue.
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
	DynamicArray<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, 1);
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		layouts.Push(m_uboDescriptorSetLayout);

	// Allocation info for descriptor sets.
	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = m_uboDescriptorPool;
	allocInfo.descriptorSetCount = layouts.GetSize();
	allocInfo.pSetLayouts = layouts.Data();

	// Resize descriptor set array.
	m_uboDescriptorSets.SetSize(MAX_FRAMES_IN_FLIGHT);
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
		VkImageView attachments[] = { m_swapChainImageViews[i], m_depthImage->ImageView() }; // Image used as the framebuffer attachment.

		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = m_mainRenderPass;
		framebufferInfo.attachmentCount = 2;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = m_swapChainImageExtents.width;
		framebufferInfo.height = m_swapChainImageExtents.height;
		framebufferInfo.layers = 1;

		RENDERER_SAFECALL(vkCreateFramebuffer(m_logicDevice, &framebufferInfo, nullptr, &m_swapChainFramebuffers[i]), "Renderer Error: Failed to create swap chain framebuffer.");
	}
}

void Renderer::CreateCommandPool() 
{
	// ==================================================================================================================================================
	// Graphics command pool.

	VkCommandPoolCreateInfo poolCreateInfo = {};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolCreateInfo.queueFamilyIndex = m_nGraphicsQueueFamilyIndex;
	poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

	RENDERER_SAFECALL(vkCreateCommandPool(m_logicDevice, &poolCreateInfo, nullptr, &m_graphicsCmdPool), "Renderer Error: Failed to create command pool.");

}

void Renderer::CreateCmdBuffers() 
{
	// ==================================================================================================================================================
	// Graphics commands.

	// Allocate handle memory for main command buffers...
	m_mainPrimaryCmdBufs.SetSize(m_swapChainFramebuffers.GetSize());
	m_mainPrimaryCmdBufs.SetCount(m_mainPrimaryCmdBufs.GetSize());

	// Allocate handle memory for dynamic command buffers...
	m_dynamicPassCmdBufs.SetSize(m_swapChainFramebuffers.GetSize());
	m_dynamicPassCmdBufs.SetCount(m_dynamicPassCmdBufs.GetSize());

	// Allocation info.
	VkCommandBufferAllocateInfo cmdBufAllocateInfo = {};
	cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdBufAllocateInfo.commandPool = m_graphicsCmdPool;
	cmdBufAllocateInfo.commandBufferCount = m_swapChainFramebuffers.GetSize();
	cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	// Allocate main command buffers.
	RENDERER_SAFECALL(vkAllocateCommandBuffers(m_logicDevice, &cmdBufAllocateInfo, m_mainPrimaryCmdBufs.Data()), "Renderer Error: Failed to allocate dynamic command buffers.");

	cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;

	// Allocate dynamic command buffers...
	RENDERER_SAFECALL(vkAllocateCommandBuffers(m_logicDevice, &cmdBufAllocateInfo, m_dynamicPassCmdBufs.Data()), "Renderer Error: Failed to allocate dynamic command buffers.");

	// These command buffers will need to be initially recorded.
	m_dynamicStateChange[0] = true;
	m_dynamicStateChange[1] = true;
	m_dynamicStateChange[2] = true;

	// ==================================================================================================================================================
	// Transfer commands

	VkCommandPoolCreateInfo transferPoolInfo = {};
	transferPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	transferPoolInfo.queueFamilyIndex = m_nTransferQueueFamilyIndex;
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
	VkCommandBuffer& cmdBuffer = m_transferCmdBufs[bufferIndex];

	VkCommandBufferBeginInfo cmdBeginInfo = {};
	cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	cmdBeginInfo.pInheritanceInfo = nullptr;

	RENDERER_SAFECALL(vkBeginCommandBuffer(cmdBuffer, &cmdBeginInfo), "Renderer Error: Failed to begin recording of transfer command buffer.");

	DynamicArray<CopyRequest>& requests = m_transferBuffers[bufferIndex];

	// Issue commands for each requested copy operation.
	for (int i = 0; i < requests.Count(); ++i)
	{
		vkCmdCopyBuffer(cmdBuffer, requests[i].srcBuffer, requests[i].dstBuffer, 1, &requests[i].copyRegion);
	}

	RENDERER_SAFECALL(vkEndCommandBuffer(cmdBuffer), "Renderer Error: Failed to end recording of transfer command buffer.");
	requests.Clear();
}

void Renderer::RecordMainCommandBuffer(const unsigned int& bufferIndex)
{
	VkClearValue clearVal = { 0.0f, 0.0f, 0.0f, 1.0f };
	VkClearValue depthClearVal = { 1.0f, 1.0f, 1.0f, 1.0f };

	VkClearValue clearVals[2] = { clearVal, depthClearVal };

	// Record primary command buffers...
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

	vkBeginCommandBuffer(m_mainPrimaryCmdBufs[bufferIndex], &beginInfo);

	// Begin render pass.
	VkRenderPassBeginInfo mainPassBeginInfo = {};
	mainPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	mainPassBeginInfo.renderPass = m_mainRenderPass;
	mainPassBeginInfo.framebuffer = m_swapChainFramebuffers[bufferIndex];
	mainPassBeginInfo.renderArea.offset = { 0, 0 };
	mainPassBeginInfo.renderArea.extent = m_swapChainImageExtents;
	mainPassBeginInfo.clearValueCount = 2;
	mainPassBeginInfo.pClearValues = clearVals;

	// Subpass commands are recorded in secondary command buffers.
	vkCmdBeginRenderPass(m_mainPrimaryCmdBufs[bufferIndex], &mainPassBeginInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

	VkCommandBuffer subpassCommands[] = { m_dynamicPassCmdBufs[bufferIndex] };

	// Execute subpass secondary command buffers.
	vkCmdExecuteCommands(m_mainPrimaryCmdBufs[bufferIndex], 1, subpassCommands);

	vkCmdEndRenderPass(m_mainPrimaryCmdBufs[bufferIndex]);

	vkEndCommandBuffer(m_mainPrimaryCmdBufs[bufferIndex]);
}

void Renderer::RecordDynamicCommandBuffer(const unsigned int& bufferIndex, const unsigned int& frameIndex)
{
	// If there was no dynamic state change, no re-recording is necessary.
	if (!m_dynamicStateChange[bufferIndex])
		return;

	VkCommandBufferBeginInfo cmdBeginInfo = {};
	cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;

	// This command buffer is a secondary command buffer, executing draw commands for the first subpass.
	VkCommandBufferInheritanceInfo inheritanceInfo = {};
	inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
	inheritanceInfo.framebuffer = m_swapChainFramebuffers[bufferIndex];
	inheritanceInfo.renderPass = m_mainRenderPass;
	inheritanceInfo.subpass = 0;
	inheritanceInfo.occlusionQueryEnable = VK_FALSE;

	cmdBeginInfo.pInheritanceInfo = &inheritanceInfo;

	// Initial recording of dynamic command buffers...
	VkCommandBuffer& cmdBuffer = m_dynamicPassCmdBufs[bufferIndex];

	RENDERER_SAFECALL(vkBeginCommandBuffer(cmdBuffer, &cmdBeginInfo), "Renderer Error: Failed to begin recording of dynamic pass command buffer.");

	DynamicArray<PipelineData*>& allPipelines = MeshRenderer::Pipelines();

	// Iterate through all pipelines and draw their objects.
	for (int i = 0; i < allPipelines.Count(); ++i)
	{
		PipelineData& currentPipeline = *allPipelines[i];

		// Bind pipelines...
		vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, currentPipeline.m_handle);

		currentPipeline.m_material->UseDescriptorSet(cmdBuffer, currentPipeline.m_layout, frameIndex);

		// Draw objects using the pipeline.
		DynamicArray<MeshRenderer*>& renderObjects = currentPipeline.m_renderObjects;
		for (int j = 0; j < renderObjects.Count(); ++j)
		{
			renderObjects[j]->UpdateInstanceData();
			renderObjects[j]->CommandDraw(cmdBuffer);
		}
	}

	RENDERER_SAFECALL(vkEndCommandBuffer(cmdBuffer), "Renderer Error: Failed to end recording of dynamic pass command buffer.");

	RecordMainCommandBuffer(bufferIndex);

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
	VkSubmitInfo transSubmitInfo = {};
	transSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	transSubmitInfo.commandBufferCount = 1;
	transSubmitInfo.waitSemaphoreCount = 0;
	transSubmitInfo.pWaitSemaphores = nullptr;
	transSubmitInfo.signalSemaphoreCount = 0;
	transSubmitInfo.pSignalSemaphores = nullptr;

	while(m_bTransferThread) 
	{
		if (!m_bTransferReady && m_transferIndices.Count() > 0) 
		{
			unsigned int transferIndex = m_transferIndices.Dequeue();

			// Set command buffer.
			transSubmitInfo.pCommandBuffers = &m_transferCmdBufs[transferIndex];

			vkWaitForFences(m_logicDevice, 1, &m_copyReadyFences[transferIndex], VK_TRUE, std::numeric_limits<unsigned long long>::max());
			vkResetFences(m_logicDevice, 1, &m_copyReadyFences[transferIndex]);

			// Record command buffer.
			RecordTransferCommandBuffer(transferIndex);

			// Execute transfer commands.
			vkQueueSubmit(m_transferQueue, 1, &transSubmitInfo, m_copyReadyFences[transferIndex]);

			m_bTransferReady = true;
		}
	}
}

void Renderer::Begin() 
{
	m_nCurrentFrameIndex = ++m_nCurrentFrame % MAX_FRAMES_IN_FLIGHT;

	// Wait for frames-in-flight.
	vkWaitForFences(m_logicDevice, 1, &m_inFlightFences[m_nCurrentFrameIndex], VK_TRUE, std::numeric_limits<unsigned long long>::max());
	vkResetFences(m_logicDevice, 1, &m_inFlightFences[m_nCurrentFrameIndex]);

	// Aquire next image from the swap chain.
	RENDERER_SAFECALL(vkAcquireNextImageKHR(m_logicDevice, m_swapChain, std::numeric_limits<unsigned long long>::max(), m_imageAvailableSemaphores[m_nCurrentFrameIndex], VK_NULL_HANDLE, &m_nPresentImageIndex),
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
	m_newTransferRequests.Push(request);
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
	UpdateMVP(m_nCurrentFrameIndex);

	// Re-record the dynamic command buffer for this swap chain image if it has changed.
    RecordDynamicCommandBuffer(m_nPresentImageIndex, m_nCurrentFrameIndex);

	if (m_bTransferReady && m_newTransferRequests.Count() > 0) 
	{
		m_nTransferFrameIndex = m_nCurrentFrame % MAX_CONCURRENT_COPIES;

		DynamicArray<CopyRequest>& requests = m_transferBuffers[m_nTransferFrameIndex];

		// Ensure there is enough room for the new requests.
		if(requests.GetSize() < m_newTransferRequests.GetSize())
			requests.SetSize(m_newTransferRequests.GetSize());

		// Move all new transfer requests to the correct location for command buffer recording and submission.
		requests.SetCount(m_newTransferRequests.Count());

		unsigned int nCopySize = sizeof(CopyRequest) * m_newTransferRequests.Count();
		memcpy_s(requests.Data(), nCopySize, m_newTransferRequests.Data(), nCopySize);

		m_newTransferRequests.Clear();

		// Enqueue index of these requests for command buffer recording and execution.
		m_transferIndices.Enqueue(m_nTransferFrameIndex);
		m_bTransferReady = false;
	}

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	VkCommandBuffer cmdBuffers[] = { m_mainPrimaryCmdBufs[m_nPresentImageIndex] };

	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &m_imageAvailableSemaphores[m_nCurrentFrameIndex];
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = cmdBuffers;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &m_renderFinishedSemaphores[m_nCurrentFrameIndex];

	RENDERER_SAFECALL(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFences[m_nCurrentFrameIndex]), "Renderer Error: Failed to submit command buffer.");

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &m_renderFinishedSemaphores[m_nCurrentFrameIndex];
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &m_swapChain;
	presentInfo.pImageIndices = &m_nPresentImageIndex;
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
	return m_mainRenderPass;
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