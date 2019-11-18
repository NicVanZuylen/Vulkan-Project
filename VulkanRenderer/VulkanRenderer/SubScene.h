#pragma once
#include <vulkan/vulkan.h>
#include "DynamicArray.h"
#include "VertexInfo.h"
#include "Renderer.h"

class RenderModule;
class GBufferPass;
class LightingManager;
class Texture;
class Material;

struct Shader;

#define SUB_PASS_COUNT 2
#define POST_SUBPASS_INDEX 2

enum EGBufferAttachmentTypeBit
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

struct SubSceneParams 
{
	Renderer* m_renderer;
	unsigned int m_nQueueFamilyIndex;
	unsigned int m_nFrameBufferWidth;
	unsigned int m_nFrameBufferHeight;
	Shader* m_dirLightShader;
	Shader* m_pointLightShader;
	EGBufferAttachmentTypeBit eAttachmentBits;
	bool m_bPrimary;
	bool m_bOutputHDR;
};

// Contains graphics pipeline handles and pointers to all renderobjects using the pipeline for the subscene it belongs to.
struct PipelineData
{
	PipelineData();

	Material* m_material;
	VkPipeline m_handle;
	VkPipelineLayout m_layout;
	DynamicArray<EVertexAttribute> m_vertexAttributes;
	DynamicArray<RenderObject*> m_renderObjects; // All objects using this pipeline.
};

struct PipelineDataPtr
{
	PipelineDataPtr();

	PipelineData* m_ptr;
};

/*
Description: A Render Pass and Collection of RenderObjects used for rendering a scene to a texture or swap chain image.
Author: Nic Van Zuylen
*/

class SubScene
{
public:

	SubScene(SubSceneParams& params);

	~SubScene();

	/*
	Description: Set which G Buffer images this Subscene will create & use. 
	Param:
	    EGBufferImageTypeBit eImageBits: Bit field of the images to create & use for rendering.
    */
	void SetImages(EGBufferAttachmentTypeBit eImageBits);

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
	Description: Create the framebuffer for this subscene.
	*/
	inline void CreateFramebuffers();

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

	EGBufferAttachmentTypeBit m_eGBufferImageBits;

	Texture* m_colorImages[MAX_FRAMES_IN_FLIGHT];
	Texture* m_depthImages[MAX_FRAMES_IN_FLIGHT];
	Texture* m_posImages[MAX_FRAMES_IN_FLIGHT];
	Texture* m_normalImages[MAX_FRAMES_IN_FLIGHT];

	DynamicArray<Texture*> m_gBufferImages[MAX_FRAMES_IN_FLIGHT];
	DynamicArray<Texture*> m_allImages[MAX_FRAMES_IN_FLIGHT];
	DynamicArray<VkClearValue> m_clearValues;

	// ---------------------------------------------------------------------------------
	// Framebuffer

	VkFramebuffer m_framebuffers[MAX_FRAMES_IN_FLIGHT];

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

	Table<PipelineDataPtr> m_pipelines;
	DynamicArray<PipelineData*> m_allPipelines; // All pipelines to iterate through when rendering.

	// ---------------------------------------------------------------------------------
	// Output

	unsigned int m_nWidth;
	unsigned int m_nHeight;

	Texture* m_outImage; // Output image of this scene.
};

