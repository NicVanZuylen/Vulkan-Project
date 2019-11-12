#pragma once
#include <vulkan/vulkan.h>
#include "DynamicArray.h"

class Renderer;
class RenderModule;
class GBufferPass;
class LightingManager;
class Texture;

#define SUB_PASS_COUNT 2
#define POST_SUBPASS_INDEX 2

enum EGBufferImageTypeBit
{
	GBUFFER_COLOR_BIT = 1,
	GBUFFER_COLOR_HDR_BIT = 1 << 1,
	GBUFFER_DEPTH_BIT = 1 << 2,
	GBUFFER_POSITION_BIT = 1 << 3,
	GBUFFER_NORMAL_BIT = 1 << 4,
};

enum EMiscGBufferType 
{
	GBUFFER_MISC_8_BIT,
	GBUFFER_MISC_16_BIT_FLOAT,
	GBUFFER_MISC_32_BIT_FLOAT
};

enum EGBufferChannels 
{
	GBUFFER_CHANNEL_R,
	GBUFFER_CHANNEL_RG,
	GBUFFER_CHANNEL_RGB,
	GBUFFER_CHANNEL_RGBA
};

struct MiscGBufferDescription 
{
	EMiscGBufferType m_eType;
	EGBufferChannels m_eChannels;
	bool m_bInput;
};

/*
Description: A Render Pass and Collection of RenderObjects used for rendering a scene to a texture or swap chain image.
Author: Nic Van Zuylen
*/

class SubScene
{
public:

	SubScene(Renderer* renderer, bool bPrimary, unsigned int nQueueFamilyIndex, unsigned int nOutWidth, unsigned int nOutHeight, EGBufferImageTypeBit eImageBits, bool bOutputHDR = false);

	~SubScene();

	/*
	Description: Set which G Buffer images this Subscene will create & use. 
	Param:
	    EGBufferImageTypeBit eImageBits: Bit field of the images to create & use for rendering.
    */
	void SetImages(EGBufferImageTypeBit eImageBits);

	/*
    Description: Draw this subscene.
	Param:
		const uint32_t nPresentImageIndex: Index of the swap chain image to render to.
		const uint32_t nFrameIndex: Index of the current frame-in-flight.
		DynamicArray<VkSemaphore>& waitSemaphores: Sempahores to wait on before executing rendering commands.
		VkPipelineStageFlags* waitStages: Array of stage maskes for wait semaphores to wait on.
		DynamicArray<VkSemaphore>& signalSemaphores: Semaphores to signal once rendering is complete.
		VkFence signalFence: Fence to signal once rendering is complete.
    */
	void DrawScene(const uint32_t& nPresentImageIndex, const uint32_t nFrameIndex, DynamicArray<VkSemaphore>& waitSemaphores, VkPipelineStageFlags* waitStages, DynamicArray<VkSemaphore>& signalSemaphores, VkFence signalFence);

	GBufferPass* GetGBufferPass();

	LightingManager* GetLightingManager();

private:

	// ---------------------------------------------------------------------------------
	// Constructor extensions

	/*
	Description: Create VkAttachmentDescription structures to aid in render pass creation.
	*/
	inline void CreateAttachmentDescriptions(const DynamicArray<Texture*>& targets, DynamicArray<VkAttachmentDescription>& attachments);

	/*
	Description: Create VkAttachmentReference structures to aid in render pass creation.
	*/
	inline void CreateAttachmentReferences(const uint32_t& nIndexOffset, const DynamicArray<Texture*>& targets, DynamicArray<VkAttachmentReference>& references);

	/*
	Description: Create input attachment VkAttachmentReference structures to aid in render pass creation.
	*/
	inline void CreateInputAttachmentReferences(const uint32_t& nIndexOffset, const DynamicArray<Texture*>& targets, DynamicArray<VkAttachmentReference>& references);

	/*
	Description: Create render pass for this subscene.
	*/
	inline void CreateRenderPass();

	/*
	Description: Create command pool & primary command buffers.
	*/
	inline void CreateCmds();

	/*
	Description: Find the queue this subscene will submit commands to.
	*/
	inline void GetQueue();

	/*
	Description: Record primary command buffer for this subscene.
	Param:
	    const uint32_t nPresentImageIndex: Index of the swap chain image to render to.
		const uint32_t nFrameIndex: Index of the current frame-in-flight.
	*/
	inline void RecordPrimaryCmdBuffer(const uint32_t& nPresentImageIndex, const uint32_t& nFrameIndex);

	// ---------------------------------------------------------------------------------
	// Vulkan structure templates

	// These are static Vulkan structure caches used to reduce the amount of code in the source file.

	static VkAttachmentDescription m_swapChainAttachmentDescription;
	static VkAttachmentDescription m_depthAttachmentDescription;
	static VkAttachmentDescription m_colorAttachmentDescription;
	static VkAttachmentDescription m_colorHDRAttachmentDescription;
	static VkAttachmentDescription m_vectorAttachmentDescription;

	static VkSubpassDependency m_gBufferDependency; // Subpass dependency for g-buffer subpass.
	static VkSubpassDependency m_lightingDependency; // Subpass dependency for lighting subpass.
	static VkSubpassDependency m_postDependency; // Subpass dependency for all post effects.

	static VkCommandBufferBeginInfo m_primaryCmdBeginInfo; // Begin info for primary command buffer recording.

	static VkSubmitInfo m_renderSubmitInfo;

	// ---------------------------------------------------------------------------------
	// Main

	Renderer* m_renderer; // Renderer reference.

	// ---------------------------------------------------------------------------------
	// Modules

	GBufferPass* m_gPass;
	LightingManager* m_lightManager;

	// ---------------------------------------------------------------------------------
	// Render target images.

	EGBufferImageTypeBit m_eGBufferImageBits;

	Texture* m_colorImage;
	Texture* m_depthImage;
	Texture* m_posImage;
	Texture* m_normalImage;

	DynamicArray<Texture*> m_targetImages;
	DynamicArray<Texture*> m_gBufferImages;

	// ---------------------------------------------------------------------------------
	// Render pass

	VkRenderPass m_pass; // Render pass handle used to render this subscene.
	unsigned int m_nQueueFamilyIndex;

	// ---------------------------------------------------------------------------------
	// Commands

	VkCommandPool m_commandPool;
	DynamicArray<VkCommandBuffer> m_primaryCmdBufs;

	VkQueue m_renderQueue;

	// ---------------------------------------------------------------------------------
	// Member objects.

	// ---------------------------------------------------------------------------------
	// Output

	unsigned int m_nWidth;
	unsigned int m_nHeight;

	Texture* m_outImage; // Output image of this scene.
};

