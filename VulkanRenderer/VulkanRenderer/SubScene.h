#pragma once
#include <vulkan/vulkan.h>
#include "DynamicArray.h"
#include "VertexInfo.h"
#include "Renderer.h"

/*
Description: A Render Pass & collection of modules & render objects for rendering a scene to a texture or swap chain image.
Author: Nic Van Zuylen
*/

class RenderModule;
class GBufferPass;
class LightingManager;
class ShadowMap;
class Texture;
class Material;

struct Shader;

#define SUB_PASS_COUNT 3
#define POST_SUBPASS_INDEX ~0

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

struct MiscGBufferDesc 
{
	EMiscGBufferType m_eType;
	glm::vec4 m_v4ClearColor;
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
	DynamicArray<MiscGBufferDesc> m_miscGAttachments; // Misc G-Buffer attachments to add.
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
	uint32_t m_nReferenceCount; // Amount of subscenes referencing this pipeline.
};

struct PipelineDataPtr
{
	PipelineDataPtr();

	PipelineData* m_ptr;
};

struct MVPUniformBuffer
{
	glm::mat4 m_view;
	glm::mat4 m_proj;
	glm::vec4 m_v4ViewPos;
};

class SubScene
{
public:

	SubScene(SubSceneParams& params);

	~SubScene();

	/*
	Description: Set which G Buffer images this Subscene will create & use. 
	Param:
	    EGBufferImageTypeBit eImageBits: Bit field of the images to create & use for rendering.
		const DynamicArray<EMiscGBufferType>& miscGBuffers: Formats for extra g buffer images to create.
    */
	void CreateImages(EGBufferAttachmentTypeBit eImageBits, const DynamicArray<MiscGBufferDesc>& miscGBuffers);

	/*
	Description: Resize the output framebuffer of this subscene
	Param:
	    uint32_t nNewWidth: The new width of the rendering output.
		uint32_t nNewHeight: The new height of the rendering output.
	*/
	void ResizeOutput(uint32_t nNewWidth, uint32_t nNewHeight);

	/*
	Description: Update the camera view matrix for this subscene.
	Param:
	    const glm::mat4& view
	*/
	void UpdateCameraView(const glm::mat4& view, const glm::vec4& v4ViewPos);

	/*
	Description: Record primary command buffer for this subscene.
	Param:
		const uint32_t nPresentImageIndex: Index of the swap chain image to render to.
		const uint32_t nFrameIndex: Index of the current frame-in-flight.
		VkCommandBuffer& transferCmdBuf: Command buffer to record all transfer commands to.
	*/
	void RecordPrimaryCmdBuffer(const uint32_t& nPresentImageIndex, const uint32_t& nFrameIndex, VkCommandBuffer& transferCmdBuf);

	/*
    Description: Add a graphics pipeline to this scene to render.
	Param:
	    PipelineData* pipeline: The pipeline to add.
	*/
	void AddPipeline(PipelineData* pipeline);

	/*
	Description: Get the command buffer handle at the specified index.
	Param:
	    const uint32_t nIndex: Frame index of the command buffer to retreive.
	*/
	VkCommandBuffer& GetCommandBuffer(const uint32_t nIndex);

	const VkRenderPass& GetRenderPass();

	VkDescriptorSetLayout MVPUBOLayout();

	Table<PipelineDataPtr>& GetPipelineTable();

	GBufferPass* GetGBufferPass();

	const uint32_t GetGBufferCount();

	LightingManager* GetLightingManager();

	Renderer* GetRenderer();

private:

	// ---------------------------------------------------------------------------------
	// Constructor extensions

	/*
	Description: Create the output image for this subscene. Gets swap chain images if this a primary subscene.
	*/
	inline void CreateOutputImage();

	/*
	Description: Create primary descriptor pool for this subscene.
	*/
	inline void CreateDescriptorPool();

	/*
    Description: Create Camera MVP UBO descriptor sets
	*/
	inline void CreateMVPUBODescriptors(bool bCreateLayout = true);

	/*
	Description: Create Camera MVP UBO descriptor sets
	*/
	inline void CreateInputAttachmentDescriptors(bool bCreateLayout = true);

	/*
	Description: Update all descriptor sets to reference the correct resources.
	*/
	inline void UpdateAllDescriptorSets();

	/*
	Description: Create MVP UBO Buffer objects.
	*/
	inline void CreateMVPUBOBuffers();

	/*
	Description: Create a VkAttachmentDescription for swap chain images.
	*/
	inline void CreateSwapChainAttachmentDesc(DynamicArray<VkAttachmentDescription>& attachments);

	/*
	Description: Create VkAttachmentDescription structures to aid in render pass creation.
	*/
	inline void CreateAttachmentDescriptions(const DynamicArray<Texture*>& targets, DynamicArray<VkAttachmentDescription>& attachments);

	/*
	Description: Create a VkAttachmentReference for swap chain images.
	*/
	inline void CreateSwapChainAttachRef(DynamicArray<VkAttachmentReference>& references);

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

	// ---------------------------------------------------------------------------------
	// Private runtime functions

	inline void UpdateMVPUBO(const VkCommandBuffer& cmdBuffer, const uint32_t& nFrameIndex);

	// ---------------------------------------------------------------------------------
	// Vulkan structure templates

	// These are static Vulkan structure caches used to reduce the amount of code in function definitions.

	static VkDescriptorPoolCreateInfo m_poolCreateInfo;

	static VkDescriptorSetLayoutBinding m_mvpUBOLayoutBinding;
	static VkDescriptorSetLayoutCreateInfo m_mvpSetLayoutInfo;

	static VkDescriptorSetLayoutBinding m_inAttachLayoutBinding;
	static VkDescriptorSetLayoutCreateInfo m_inAttachSetLayoutInfo;
	
	static VkDescriptorSetAllocateInfo m_descAllocInfo; // Descriptor alloc info for descriptors for each frame in flight.

	static VkAttachmentDescription m_swapChainAttachmentDescription;
	static VkAttachmentDescription m_depthAttachmentDescription;
	static VkAttachmentDescription m_colorAttachmentDescription;
	static VkAttachmentDescription m_colorHDRAttachmentDescription;
	static VkAttachmentDescription m_vectorAttachmentDescription;

	static VkSubpassDependency m_shadowMapDependency; // Subpass dependency for the shadow mapping pass.
	static VkSubpassDependency m_gBufferDependency; // Subpass dependency for g-buffer subpass.
	static VkSubpassDependency m_lightingDependency; // Subpass dependency for lighting subpass.
	static VkSubpassDependency m_postDependency; // Subpass dependency for all post effects.

	static VkCommandBufferBeginInfo m_primaryCmdBeginInfo; // Begin info for primary command buffer recording.

	static VkSubmitInfo m_renderSubmitInfo;

	// ---------------------------------------------------------------------------------
	// Main

	Renderer* m_renderer; // Renderer reference.

	// ---------------------------------------------------------------------------------
	// Descriptors

	VkDescriptorPool m_descPool;

	// ---------------------------------------------------------------------------------
	// Camera

	MVPUniformBuffer m_localMVPData;

	// Camera UBOs
	VkBuffer m_mvpUBOStagingBuffers[MAX_FRAMES_IN_FLIGHT];
	VkDeviceMemory m_mvpUBOStagingMemories[MAX_FRAMES_IN_FLIGHT];

	VkBuffer m_mvpUBOBuffers[MAX_FRAMES_IN_FLIGHT];
	VkDeviceMemory m_mvpUBOMemories[MAX_FRAMES_IN_FLIGHT];

	// UBO descriptors.
	VkDescriptorSetLayout m_mvpUBOSetLayout;
	VkDescriptorSet m_mvpUBODescSets[MAX_FRAMES_IN_FLIGHT];

	// ---------------------------------------------------------------------------------
	// Modules

	ShadowMap* m_shadowMapModule;
	GBufferPass* m_gPass;
	LightingManager* m_lightManager;

	// ---------------------------------------------------------------------------------
	// Render target images.

	EGBufferAttachmentTypeBit m_eGBufferImageBits;
	DynamicArray<MiscGBufferDesc> m_miscGAttachments;

	Texture* m_shadowMapImage;
	Texture* m_colorImage;
	Texture* m_depthImage;
	Texture* m_posImage;
	Texture* m_normalImage;

	DynamicArray<Texture*> m_gBufferImages;
	DynamicArray<Texture*> m_allImages;
	DynamicArray<VkClearValue> m_clearValues;

	// ---------------------------------------------------------------------------------
	// Render target image descriptors

	VkDescriptorSetLayout m_gBufferSetLayout;
	VkDescriptorSetLayout m_outputSetLayout;

	// Descriptor set for the G Buffer input attachments, used primarily in the lighting pass.
	VkDescriptorSet m_gBufferDescSet;

	// Descriptor set for subscene output input attachment (if not rendering to swap chain image).
	VkDescriptorSet m_outputDescSet;

	// ---------------------------------------------------------------------------------
	// Framebuffer

	DynamicArray<VkImageView> m_swapchainImageViews;
	DynamicArray<VkFramebuffer> m_framebuffers;

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

	bool m_bPrimary; // Whether or not this is a primary subscene (renders to swap chain image).
	bool m_bOutputHDR; // Whether or not the output image is a HDR image.
};

