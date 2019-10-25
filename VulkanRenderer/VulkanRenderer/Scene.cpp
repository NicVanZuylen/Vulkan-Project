#include "Scene.h"
#include "Renderer.h"
#include "Texture.h"

Scene::Scene(uint32_t nWindowWidth, uint32_t nWindowHeight, uint32_t nQueueFamilyIndex)
{
	m_nWindowWidth = nWindowWidth;
	m_nWindowHeight = nWindowHeight;

	m_nGraphicsQueueFamilyIndex = nQueueFamilyIndex;

	ConstructRenderTargets();
}

Scene::~Scene() 
{
	// Destroy primary command buffers.
	vkDestroyCommandPool(m_renderer->GetDevice(), m_cmdPool, nullptr);

	// Destroy shared render target images.
	delete m_normalImage;
	delete m_posImage;
	delete m_colorImage;
	delete m_depthImage;
}

void Scene::AddSubscene()
{
	SubScene* newSubscene = new SubScene(m_renderer, m_nGraphicsQueueFamilyIndex, m_nWindowWidth, m_nWindowHeight, m_subScenes.Count() == 0);
	newSubscene->SetImages(m_colorImage, m_depthImage, m_posImage, m_normalImage); // Share render target images.

	m_subScenes.Push(newSubscene);
}

inline void Scene::ConstructSubScenes()
{
	m_subScenes.SetSize(MAX_SUBSCENE_COUNT);
	m_subScenes.SetCount(MAX_SUBSCENE_COUNT);
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

