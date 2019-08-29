#pragma once
#include <vulkan/vulkan.h>
#include <iostream>
#include <exception>

#include "DynamicArray.h"

#ifndef RENDERER_HELPER
#define RENDERER_HELPER

#ifdef RENDERER_DEBUG

#define RENDERER_SAFECALL(func, message) if(func) { throw std::runtime_error(message); }

#else

#define RENDERER_SAFECALL(func, message) func; //message

#endif

class RendererHelper 
{
public:

	struct SwapChainDetails
	{
		VkSurfaceCapabilitiesKHR m_capabilities;
		DynamicArray<VkSurfaceFormatKHR> m_formats;
		DynamicArray<VkPresentModeKHR> m_presentModes;
	};

	enum EQueueFamilyFlags
	{
		QUEUE_FAMILY_PRESENT,
		QUEUE_FAMILY_GRAPHICS,
		QUEUE_FAMILY_COMPUTE,
		QUEUE_FAMILY_TRANSFER
	};

	struct QueueFamilyIndices 
	{
		uint32_t m_nPresentFamilyIndex;
		uint32_t m_nGraphicsFamilyIndex;
		uint32_t m_nComputeFamilyIndex;
		uint32_t m_nTransferFamilyIndex;

		EQueueFamilyFlags m_eFoundQueueFamilies;
		bool m_bAllFamiliesFound;
	};

	/*
	Description: Check whether or not the provided physical device meets standard game graphical capabilities, and rate suitability with a score output.
	Return Type: int
	Param:
		VkSurfaceKHR windowSurface: The window surface the swap chain will present to.
		VkPhysicalDevice device: The physical device to query.
	    const DynamicArray<const char*>& extensionNames: Names of Vulkan extensions required.
		EQueueFamilyFlags desiredQueueFamilies: The desired queue families to be found on the phyiscal device.
	*/
	static int DeviceSuitable(VkSurfaceKHR windowSurface, VkPhysicalDevice device, const DynamicArray<const char*>& extensionNames, EQueueFamilyFlags eDesiredQueueFamilies);

	/*
	Description: Check if the provided physical device supports the provided extensions.
	Return Type: bool
	Param:
	    const DynamicArray<const char*>& extensionNames: Names of the desired extensions.
		VkPhysicalDevice device: The physical device to check.
	*/
	static bool CheckDeviceExtensionSupport(const DynamicArray<const char*>& extensionNames, VkPhysicalDevice device);

	/*
	Description: Retreive information on queue families for the provided device and window surface.
	Return Type: QueueFamilyIndices
	Param:
	    VkSurfaceKHR windowSurface: The window surface to check.
		VkPhysicalDevice device: The physical device to check.
		EQueueFamilyFlags eDesiredFamilies: The desired queue families to find.
	*/
	static QueueFamilyIndices FindQueueFamilies(VkSurfaceKHR windowSurface, VkPhysicalDevice device, EQueueFamilyFlags eDesiredFamilies);

	/*
	Description: Retreive swap chain information from the provided window surface and physical device.
	Return Type: SwapChainDetails* (& Memory Ownership)
	Param:
		VkSurfaceKHR windowSurface: The window surface to check.
		VkPhysicalDevice device: The physical device to check.
	*/
	static SwapChainDetails* GetSwapChainSupportDetails(VkSurfaceKHR windowSurface, VkPhysicalDevice device);



	/*
	Description: Find the best supported image format out of the provided image formats.
	Return Type: VkFormat
	Param:
	    VkPhysicalDevice physDevice: The physical device the formats should be available on.
		DynamicArray<VkFormat>& formats: The pool of formats to search for the best one.
		VkImageTiling tiling: Specify the image tiling method used.
		VkFormatFeatureFlags features: Desired features formats should possess.
	*/
	static VkFormat FindBestDepthFormat(VkPhysicalDevice physDevice, DynamicArray<VkFormat>& formats, VkImageTiling tiling, VkFormatFeatureFlags features);

	template<typename T>
	static T GetExternalFunction(VkInstance instance, const char* funcName)
	{
		auto func = (T)vkGetInstanceProcAddr(instance, funcName);

		return func;
	}

	// Debug messenger creation proxy function.
	static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* createInfo, const VkAllocationCallbacks* allocator, VkDebugUtilsMessengerEXT* messenger);

	// Debug messenger destruction proxy function.
	static VkResult DestroyDebugUtilsMessengerEXT(VkInstance instance, const VkAllocationCallbacks* allocator, VkDebugUtilsMessengerEXT* messenger);

	static VKAPI_ATTR VkBool32 VKAPI_CALL ErrorCallback
	(
		VkDebugUtilsMessageSeverityFlagBitsEXT severity,
		VkDebugUtilsMessageTypeFlagsEXT type,
		const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
		void* userData
	);

	/*
	Description: Create validation layer messenger.
	Param:
	    const VkInstance& instance: The Vulkan instance to create the messenger for.
		VkDebugUtilsMessengerEXT& messenger: Messenger handle to reference with.
	*/
	static void SetupDebugMessenger(const VkInstance& instance, VkDebugUtilsMessengerEXT& messenger);
};

#endif
