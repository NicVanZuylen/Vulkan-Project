#include "Scene.h"

#undef MAX_FRAMES_IN_FLIGHT // Undefine duplicate used in header.
#include "Renderer.h"

#include "SubScene.h"
#include "Shader.h"

Scene::Scene(Renderer* renderer, uint32_t nWindowWidth, uint32_t nWindowHeight, uint32_t nQueueFamilyIndex)
{
	m_renderer = renderer;

	m_nWindowWidth = nWindowWidth;
	m_nWindowHeight = nWindowHeight;

	m_dirLightShader = new Shader(m_renderer, FS_QUAD_SHADER, DEFERRED_DIR_LIGHT_SHADER);
	m_pointLightShader = new Shader(m_renderer, POINT_LIGHT_VERTEX_SHADER, DEFERRED_POINT_LIGHT_SHADER);

	m_nQueueFamilyIndex = nQueueFamilyIndex;

	GetQueue();
	CreateTransferCmdPool();
	AllocateTransferCmdBufs();
	CreateSyncObjects();

	SubSceneParams params;
	params.eAttachmentBits = (EGBufferAttachmentTypeBit)(GBUFFER_COLOR_BIT | GBUFFER_COLOR_HDR_BIT | GBUFFER_DEPTH_BIT | GBUFFER_POSITION_BIT | GBUFFER_NORMAL_BIT);
	params.m_bOutputHDR = false;
	params.m_bPrimary = true;
	params.m_dirLightShader = m_dirLightShader;
	params.m_pointLightShader = m_pointLightShader;
	params.m_nFrameBufferWidth = m_nWindowWidth;
	params.m_nFrameBufferHeight = m_nWindowHeight;
	params.m_nQueueFamilyIndex = m_nQueueFamilyIndex;
	params.m_renderer = m_renderer;

	m_primarySubscene = new SubScene(params);
}

Scene::~Scene() 
{
	VkDevice device = m_renderer->GetDevice();

	// Device needs to be idle before destroying resources it may otherwise be using.
	vkDeviceWaitIdle(device);

	// Destroy subscenes.
	delete m_primarySubscene;

	// Destroy shared shaders.
	delete m_dirLightShader;
	delete m_pointLightShader;

	// Destroy command pool.
	vkDestroyCommandPool(device, m_transferCmdPool, nullptr);

	// Destroy sync objects.
	for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) 
	{
		vkDestroySemaphore(device, m_renderFinishedSemaphores[i], nullptr);
		vkDestroySemaphore(device, m_transferCompleteSemaphores[i], nullptr);
	}
}

void Scene::ResizeOutput(const uint32_t nWidth, const uint32_t nHeight)
{
	VkDevice device = m_renderer->GetDevice();

	// Reset sync objects.
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		vkDestroySemaphore(device, m_renderFinishedSemaphores[i], nullptr);
		vkDestroySemaphore(device, m_transferCompleteSemaphores[i], nullptr);
	}

	CreateSyncObjects();

	m_primarySubscene->ResizeOutput(nWidth, nHeight);
}

void Scene::UpdateCameraView(const glm::mat4& view, const glm::vec4& v4ViewPos)
{
	m_primarySubscene->UpdateCameraView(view, v4ViewPos);
}

SubScene* Scene::GetPrimarySubScene()
{
	return m_primarySubscene;
}

Renderer* Scene::GetRenderer()
{
	return m_renderer;
}

void Scene::DrawSubscenes(const uint32_t& nPresentImageIndex, const uint64_t nElapsedFrames, const uint32_t& nFrameIndex, VkSemaphore& imageAvailableSemaphore, VkSemaphore& renderFinishedSemaphore, VkFence& frameFence)
{
	// ---------------------------------------------------------------------------------
	// Frame indices

	uint32_t nTransferFrameIndex = (nElapsedFrames - 2) % MAX_FRAMES_IN_FLIGHT;

	VkDevice device = m_renderer->GetDevice();

	// ---------------------------------------------------------------------------------
	// Begin recording of transfer commands

	VkCommandBufferBeginInfo transferCmdBeginInfo = {};
	transferCmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	transferCmdBeginInfo.pNext = nullptr;
	transferCmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	transferCmdBeginInfo.pInheritanceInfo = nullptr;

	RENDERER_SAFECALL(vkBeginCommandBuffer(m_transferCmdBufs[nFrameIndex], &transferCmdBeginInfo), "Scene Error: Failed to begin recording of transfer command buffer.");

	// ---------------------------------------------------------------------------------
	// Record & Submit subscenes

	VkPipelineStageFlags renderWaitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  VK_PIPELINE_STAGE_TRANSFER_BIT }; // Wait for transfers to complete first before submitting render commands.

	// Record subscene command bufer.
	m_primarySubscene->RecordPrimaryCmdBuffer(nPresentImageIndex, nFrameIndex, m_transferCmdBufs[nFrameIndex]);

	// Finish recording of transfer commands.
	RENDERER_SAFECALL(vkEndCommandBuffer(m_transferCmdBufs[nFrameIndex]), "Scene Error: Failed to end recording of transfer command buffer.");

	VkSemaphore renderWaitSemaphores[] = { imageAvailableSemaphore, m_transferCompleteSemaphores[nTransferFrameIndex] };

	// Set render finished semaphore reference, used outside this function to wait for rendering to finish before aquiring the next swap chain image.
	renderFinishedSemaphore = m_renderFinishedSemaphores[nFrameIndex];

	VkSubmitInfo renderSubmitInfo = {};
	renderSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	renderSubmitInfo.pNext = nullptr;
	renderSubmitInfo.waitSemaphoreCount = 1 + (nElapsedFrames > 1); // Only wait on second semaphore after the first frame.
	renderSubmitInfo.pWaitSemaphores = renderWaitSemaphores;
	renderSubmitInfo.pWaitDstStageMask = renderWaitStages;
	renderSubmitInfo.signalSemaphoreCount = 1;
	renderSubmitInfo.pSignalSemaphores = &m_renderFinishedSemaphores[nFrameIndex];
	renderSubmitInfo.commandBufferCount = 1;
	renderSubmitInfo.pCommandBuffers = &m_primarySubscene->GetCommandBuffer(nFrameIndex);

	// ---------------------------------------------------------------------------------
	// Submit transfer commands

	VkSubmitInfo transSubmitInfo;
	transSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	transSubmitInfo.pNext = nullptr;
	transSubmitInfo.waitSemaphoreCount = 0;
	transSubmitInfo.pWaitSemaphores = nullptr;
	transSubmitInfo.pWaitDstStageMask = nullptr;
	transSubmitInfo.signalSemaphoreCount = 1;
	transSubmitInfo.pSignalSemaphores = &m_transferCompleteSemaphores[nFrameIndex];
	transSubmitInfo.commandBufferCount = 1;
	transSubmitInfo.pCommandBuffers = &m_transferCmdBufs[nFrameIndex];

	// Submit transfer commands...
	RENDERER_SAFECALL(vkQueueSubmit(m_queue, 1, &transSubmitInfo, VK_NULL_HANDLE), "Scene Error: Failed to submit transfer commands.");

	// Submit rendering commands for execution...
	RENDERER_SAFECALL(vkQueueSubmit(m_queue, 1, &renderSubmitInfo, frameFence), "Scene Error: Failed to submit render commands.");
}

inline void Scene::GetQueue()
{
	// Obtain queue to submit commands to.
	vkGetDeviceQueue(m_renderer->GetDevice(), m_nQueueFamilyIndex, 0, &m_queue);
}

inline void Scene::CreateTransferCmdPool()
{
	VkCommandPoolCreateInfo cmdPoolInfo = {};
	cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolInfo.pNext = nullptr;
	cmdPoolInfo.queueFamilyIndex = m_nQueueFamilyIndex;
	cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

	RENDERER_SAFECALL(vkCreateCommandPool(m_renderer->GetDevice(), &cmdPoolInfo, nullptr, &m_transferCmdPool), "Scene Error: Failed to create transfer command pool!");
}

inline void Scene::AllocateTransferCmdBufs()
{
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = m_transferCmdPool;
	allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.pNext = nullptr;

	RENDERER_SAFECALL(vkAllocateCommandBuffers(m_renderer->GetDevice(), &allocInfo, m_transferCmdBufs), "Scene Error: Failed to allocate transfer command buffers.");
}

inline void Scene::CreateSyncObjects()
{
	VkSemaphoreCreateInfo semaphoreCreateInfo = 
	{
		VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		nullptr,
		0
	};

	VkDevice device = m_renderer->GetDevice();

	// Create sempahores...
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &m_renderFinishedSemaphores[i]);
		vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &m_transferCompleteSemaphores[i]);
	}
}

