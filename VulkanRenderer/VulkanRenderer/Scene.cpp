#include "Scene.h"
#include "Renderer.h"
#include "Texture.h"
#include "Shader.h"

Scene::Scene(uint32_t nWindowWidth, uint32_t nWindowHeight, uint32_t nQueueFamilyIndex)
{
	m_nWindowWidth = nWindowWidth;
	m_nWindowHeight = nWindowHeight;

	m_dirLightShader = new Shader(m_renderer, FS_QUAD_SHADER, DEFERRED_DIR_LIGHT_SHADER);
	m_pointLightShader = new Shader(m_renderer, FS_QUAD_SHADER, DEFERRED_POINT_LIGHT_SHADER);

	m_nGraphicsQueueFamilyIndex = nQueueFamilyIndex;

	ConstructRenderTargets();
}

Scene::~Scene() 
{
	// Destroy primary command buffers.
	vkDestroyCommandPool(m_renderer->GetDevice(), m_cmdPool, nullptr);

	// Destroy shared shaders.
	delete m_dirLightShader;
	delete m_pointLightShader;

	// Destroy shared render target images.
	delete m_normalImage;
	delete m_posImage;
	delete m_colorImage;
	delete m_depthImage;
}

void Scene::AddSubscene()
{
	EGBufferAttachmentTypeBit eImageBits = (EGBufferAttachmentTypeBit)(GBUFFER_COLOR_BIT | GBUFFER_DEPTH_BIT | GBUFFER_POSITION_BIT | GBUFFER_NORMAL_BIT);

	// Set up subscene constructor parameters...
	SubSceneParams params = {};
	params.eAttachmentBits = eImageBits;
	params.m_bOutputHDR = true;
	params.m_bPrimary = true;
	params.m_dirLightShader = m_dirLightShader;
	params.m_pointLightShader = m_pointLightShader;
	params.m_nFrameBufferWidth = m_nWindowWidth;
	params.m_nFrameBufferHeight = m_nWindowHeight;
	params.m_nQueueFamilyIndex = m_nGraphicsQueueFamilyIndex;
	params.m_renderer = m_renderer;

	// Create subscene.
	m_subScene = new SubScene(params);
}

void Scene::DrawSubscenes(const uint32_t& nPresentImageIndex, const uint32_t nFrameIndex, DynamicArray<VkSemaphore>& waitSemaphores, DynamicArray<VkSemaphore>& renderFinishedSemaphores, VkFence& frameFence)
{
	VkPipelineStageFlags waitStages[2] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT };

	m_subScene->DrawScene(nPresentImageIndex, nFrameIndex, waitSemaphores, waitStages, renderFinishedSemaphores, frameFence);
}

inline void Scene::ConstructSubScenes()
{
	
}

inline void Scene::ConstructRenderTargets()
{
	// Specify pool of depth formats and find the best available format.
	DynamicArray<VkFormat> formats = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
	VkFormat depthFormat = RendererHelper::FindBestDepthFormat(m_renderer->GetPhysDevice(), formats, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

	// Create the depth buffer image.
	m_depthImage = new Texture(m_renderer, m_nWindowWidth, m_nWindowHeight, ATTACHMENT_DEPTH_STENCIL, depthFormat);

	// Create G Buffer images.
	m_colorImage = new Texture(m_renderer, m_nWindowWidth, m_nWindowHeight, ATTACHMENT_COLOR, VK_FORMAT_R8G8B8A8_UNORM, true); // 8-bit 4 channel RGBA color buffer.
	m_posImage = new Texture(m_renderer, m_nWindowWidth, m_nWindowHeight, ATTACHMENT_COLOR, VK_FORMAT_R16G16B16A16_SFLOAT, true); // Signed 16-bit 4-channel float buffer.
	m_normalImage = new Texture(m_renderer, m_nWindowWidth, m_nWindowHeight, ATTACHMENT_COLOR, VK_FORMAT_R16G16B16A16_SFLOAT, true); // Signed 16-bit 4-channel float buffer.
}

inline void Scene::CreateCommandPool()
{
	VkCommandPoolCreateInfo cmdPoolCreatInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		nullptr, 
		VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
		m_nGraphicsQueueFamilyIndex
	};

	RENDERER_SAFECALL(vkCreateCommandPool(m_renderer->GetDevice(), &cmdPoolCreatInfo, nullptr, &m_cmdPool), "Scene Error: Failed to create scene command pool.");
}

inline void Scene::AllocateCmdBuffers()
{
	m_primaryCmdBuffers.SetSize(m_renderer->SwapChainImageCount());
	m_primaryCmdBuffers.SetCount(m_primaryCmdBuffers.GetSize());

	VkCommandBufferAllocateInfo allocInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		nullptr,
		m_cmdPool,
		VK_COMMAND_BUFFER_LEVEL_SECONDARY,
		m_primaryCmdBuffers.Count()
	};

	RENDERER_SAFECALL(vkAllocateCommandBuffers(m_renderer->GetDevice(), &allocInfo, m_primaryCmdBuffers.Data()), "Scene Error: Failed to allocate primary command buffers.");
}

