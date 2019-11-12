#include "SubScene.h"
#include "Renderer.h"
#include "GBufferPass.h"
#include "LightingManager.h"
#include "Texture.h"

VkAttachmentDescription SubScene::m_swapChainAttachmentDescription =
{
	0,
	VK_FORMAT_R8G8B8A8_UNORM,
	VK_SAMPLE_COUNT_1_BIT,
	VK_ATTACHMENT_LOAD_OP_DONT_CARE,
	VK_ATTACHMENT_STORE_OP_STORE,
	VK_ATTACHMENT_LOAD_OP_DONT_CARE,
	VK_ATTACHMENT_STORE_OP_DONT_CARE,
	VK_IMAGE_LAYOUT_UNDEFINED,
	VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
};

VkAttachmentDescription SubScene::m_depthAttachmentDescription =
{
	0,
	VK_FORMAT_D16_UNORM_S8_UINT,
	VK_SAMPLE_COUNT_1_BIT,
	VK_ATTACHMENT_LOAD_OP_CLEAR,
	VK_ATTACHMENT_STORE_OP_DONT_CARE,
	VK_ATTACHMENT_LOAD_OP_CLEAR,
	VK_ATTACHMENT_STORE_OP_STORE,
	VK_IMAGE_LAYOUT_UNDEFINED,
	VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
};

VkAttachmentDescription SubScene::m_colorAttachmentDescription =
{
	0,
	VK_FORMAT_R8G8B8A8_UNORM,
	VK_SAMPLE_COUNT_1_BIT,
	VK_ATTACHMENT_LOAD_OP_CLEAR,
	VK_ATTACHMENT_STORE_OP_STORE,
	VK_ATTACHMENT_LOAD_OP_DONT_CARE,
	VK_ATTACHMENT_STORE_OP_DONT_CARE,
	VK_IMAGE_LAYOUT_UNDEFINED,
	VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
};

VkAttachmentDescription SubScene::m_colorHDRAttachmentDescription =
{
	0,
	VK_FORMAT_R16G16B16A16_SFLOAT,
	VK_SAMPLE_COUNT_1_BIT,
	VK_ATTACHMENT_LOAD_OP_CLEAR,
	VK_ATTACHMENT_STORE_OP_STORE,
	VK_ATTACHMENT_LOAD_OP_DONT_CARE,
	VK_ATTACHMENT_STORE_OP_DONT_CARE,
	VK_IMAGE_LAYOUT_UNDEFINED,
	VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
};

VkAttachmentDescription SubScene::m_vectorAttachmentDescription =
{
	0,
	VK_FORMAT_R16G16B16A16_SFLOAT,
	VK_SAMPLE_COUNT_1_BIT,
	VK_ATTACHMENT_LOAD_OP_CLEAR,
	VK_ATTACHMENT_STORE_OP_STORE,
	VK_ATTACHMENT_LOAD_OP_DONT_CARE,
	VK_ATTACHMENT_STORE_OP_DONT_CARE,
	VK_IMAGE_LAYOUT_UNDEFINED,
	VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
};

VkSubpassDependency SubScene::m_gBufferDependency =
{
	VK_SUBPASS_EXTERNAL,
	DYNAMIC_SUBPASS_INDEX,
	VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
	VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
	0,
	VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
	| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
};

VkSubpassDependency SubScene::m_lightingDependency =
{
	DYNAMIC_SUBPASS_INDEX,
	LIGHTING_SUBPASS_INDEX,
	VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
	VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
	VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
	VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
};

VkSubpassDependency SubScene::m_postDependency =
{
	LIGHTING_SUBPASS_INDEX,
	POST_SUBPASS_INDEX,
	VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
	VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
	VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
	VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
};

VkCommandBufferBeginInfo SubScene::m_primaryCmdBeginInfo =
{
	VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
	nullptr,
	VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
	nullptr
};

VkSubmitInfo SubScene::m_renderSubmitInfo
{
	VK_STRUCTURE_TYPE_SUBMIT_INFO,
	nullptr,
	0,
	nullptr,
	nullptr,
	1,
	nullptr,
	0,
	nullptr
};

SubScene::SubScene(Renderer* renderer, bool bPrimary, unsigned int nQueueFamilyIndex, unsigned int nOutWidth, unsigned int nOutHeight, EGBufferImageTypeBit eImageBits, bool bOutputHDR)
{
	m_renderer = renderer;

	//m_gPass = new GBufferPass(m_renderer, nQueueFamilyIndex, false);
	//m_lightManager = new LightingManager(m_renderer, nullptr, nullptr, nOutWidth, nOutHeight);

	m_pass = VK_NULL_HANDLE;
	m_nQueueFamilyIndex = nQueueFamilyIndex;

	// Create output color image. HDR if specified.
	if(!bOutputHDR)
	    m_outImage = new Texture(m_renderer, nOutWidth, nOutHeight, ATTACHMENT_COLOR, VK_FORMAT_R8G8B8A8_UNORM, !bPrimary);
	else
		m_outImage = new Texture(m_renderer, nOutWidth, nOutHeight, ATTACHMENT_COLOR, VK_FORMAT_R16G16B16A16_SFLOAT, !bPrimary);

	m_nWidth = nOutWidth;
	m_nHeight = nOutHeight;


	// Create G Buffer images.
	SetImages(eImageBits);

	// Additional operations.
	CreateRenderPass(); // Create subscene render pass.
	CreateCmds(); // Create command pool & primary command buffers.
	GetQueue(); // Get device queue subscene commands will be submitted to.
}

SubScene::~SubScene() 
{
	// Destroy command pool.
	vkDestroyCommandPool(m_renderer->GetDevice(), m_commandPool, nullptr);

	// Remove existing G Buffer images.
	for (uint32_t i = 0; i < m_gBufferImages.Count(); ++i)
		delete m_gBufferImages[i];

	// Destroy subscene render pass.
	vkDestroyRenderPass(m_renderer->GetDevice(), m_pass, nullptr);

	if (m_depthImage)
		delete m_depthImage;

	m_colorImage = nullptr;
	m_depthImage = nullptr;
	m_posImage = nullptr;
	m_normalImage = nullptr;

	delete m_gPass;
	delete m_lightManager;

	delete m_outImage;
}

void SubScene::SetImages(EGBufferImageTypeBit eImageBits)
{
	// Remove existing G Buffer images.
	for (uint32_t i = 0; i < m_gBufferImages.Count(); ++i)
		delete m_gBufferImages[i];

	m_gBufferImages.Clear();

	// Create images the bit field contains...
	if (eImageBits & GBUFFER_COLOR_BIT)
	{
		// Create HDR image if correct bit is found.
		if(eImageBits & GBUFFER_COLOR_HDR_BIT)
		    m_colorImage = new Texture(m_renderer, m_nWidth, m_nHeight, ATTACHMENT_COLOR, VK_FORMAT_R16G16B16A16_SFLOAT, true);
		else
			m_colorImage = new Texture(m_renderer, m_nWidth, m_nHeight, ATTACHMENT_COLOR, VK_FORMAT_R8G8B8A8_UNORM, true);

		m_gBufferImages.Push(m_colorImage);
	}

	if (eImageBits & GBUFFER_DEPTH_BIT) 
	{
		// Specify pool of depth formats and find the best available format.
		DynamicArray<VkFormat> depthFormats = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
		VkFormat depthFormat = RendererHelper::FindBestDepthFormat(m_renderer->GetPhysDevice(), depthFormats, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

	    m_depthImage = new Texture(m_renderer, m_nWidth, m_nHeight, ATTACHMENT_DEPTH_STENCIL, depthFormat);;
	}

	if (eImageBits & GBUFFER_POSITION_BIT) 
	{
	    m_posImage = new Texture(m_renderer, m_nWidth, m_nHeight, ATTACHMENT_COLOR, VK_FORMAT_R16G16B16A16_SFLOAT, true);
		m_gBufferImages.Push(m_posImage);
	}

	if (eImageBits & GBUFFER_NORMAL_BIT) 
	{
	    m_normalImage = new Texture(m_renderer, m_nWidth, m_nHeight, ATTACHMENT_COLOR, VK_FORMAT_R16G16B16A16_SFLOAT, true);
		m_gBufferImages.Push(m_normalImage);
	}
	
	// Store image bit field information.
	m_eGBufferImageBits = eImageBits;
}

void SubScene::DrawScene
(
	const uint32_t& nPresentImageIndex, 
	const uint32_t nFrameIndex, 
	DynamicArray<VkSemaphore>& waitSemaphores, 
	VkPipelineStageFlags* waitStages, 
	DynamicArray<VkSemaphore>& signalSemaphores, 
	VkFence signalFence
)
{
	// Record command buffers...
	RecordPrimaryCmdBuffer(nPresentImageIndex, nFrameIndex);

	// Set command buffer to submit. Submit the command buffer for the current frame-in-flight.
	m_renderSubmitInfo.pCommandBuffers = &m_primaryCmdBufs[nFrameIndex];
	m_renderSubmitInfo.waitSemaphoreCount = waitSemaphores.Count();
	m_renderSubmitInfo.pWaitSemaphores = waitSemaphores.Data();
	m_renderSubmitInfo.signalSemaphoreCount = signalSemaphores.Count();
	m_renderSubmitInfo.pSignalSemaphores = signalSemaphores.Data();
	m_renderSubmitInfo.pWaitDstStageMask = waitStages;

	// Submit commands...
	vkQueueSubmit(m_renderQueue, 1, &m_renderSubmitInfo, VK_NULL_HANDLE);
}

GBufferPass* SubScene::GetGBufferPass()
{
	return m_gPass;
}

LightingManager* SubScene::GetLightingManager()
{
	return m_lightManager;
}

inline void SubScene::CreateAttachmentDescriptions(const DynamicArray<Texture*>& targets, DynamicArray<VkAttachmentDescription>& attachments)
{
	// Reserve memory in array...
	attachments.SetSize(targets.Count());
	attachments.SetCount(attachments.GetSize());

	// Assign description based on texture attachment type.
	for (uint32_t i = 0; i < targets.Count(); ++i)
	{
		switch (targets[i]->GetAttachmentType())
		{
		case ATTACHMENT_COLOR:

			// Assign HDR description if using HDR.
			if(m_eGBufferImageBits & GBUFFER_COLOR_HDR_BIT)
			    attachments[i] = m_colorHDRAttachmentDescription;
			else
				attachments[i] = m_colorAttachmentDescription;

			break;

		case ATTACHMENT_DEPTH_STENCIL:
			attachments[i] = m_depthAttachmentDescription;
			break;

		case ATTACHMENT_SWAP_CHAIN:
			attachments[i] = m_swapChainAttachmentDescription;
			break;

		default:
			RENDERER_SAFECALL(true, "Scene: Invalid Enum!");
			break;
		}
	}
}

inline void SubScene::CreateAttachmentReferences(const uint32_t& nIndexOffset, const DynamicArray<Texture*>& targets, DynamicArray<VkAttachmentReference>& references)
{
	references.SetSize(targets.Count());
	references.SetCount(references.GetSize());

	VkAttachmentReference ref = {};

	// Set reference layouts based on attachment type.
	for (uint32_t i = 0; i < targets.Count(); ++i)
	{
		ref.attachment = nIndexOffset + i;

		switch (targets[i]->GetAttachmentType())
		{
		case ATTACHMENT_COLOR:
			ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			break;

		case ATTACHMENT_DEPTH_STENCIL:
			ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			break;

		case ATTACHMENT_SWAP_CHAIN:
			ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			break;

		default:
			RENDERER_SAFECALL(true, "Scene: Invalid Enum!");
			break;
		}

		references[i] = ref;
	}
}

inline void SubScene::CreateInputAttachmentReferences(const uint32_t& nIndexOffset, const DynamicArray<Texture*>& targets, DynamicArray<VkAttachmentReference>& references)
{
	references.SetSize(targets.Count());
	references.SetCount(references.GetSize());

	VkAttachmentReference ref = { nIndexOffset, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

	for (uint32_t i = 0; i < targets.Count(); ++i)
	{
		references[i] = ref;
		++ref.attachment;
	}
}

inline void SubScene::CreateRenderPass()
{
	// ---------------------------------------------------------------------------------
	// Delete old render pass.
	if(m_pass) 
	{
		vkDestroyRenderPass(m_renderer->GetDevice(), m_pass, nullptr);
		m_pass = VK_NULL_HANDLE;
	}

	// ---------------------------------------------------------------------------------
	// Descriptions

	m_targetImages = { m_outImage };

	DynamicArray<VkAttachmentDescription> targetDescriptions;
	CreateAttachmentDescriptions(m_targetImages, targetDescriptions);

	DynamicArray<VkAttachmentDescription> gDescriptions;
	CreateAttachmentDescriptions(m_gBufferImages, gDescriptions);

	DynamicArray<Texture*> depthImages = { m_depthImage };

	DynamicArray<VkAttachmentDescription> depthDescriptions;
	CreateAttachmentDescriptions(depthImages, depthDescriptions);

	DynamicArray<VkAttachmentDescription> allDescriptions = targetDescriptions + gDescriptions + depthDescriptions;

	// ---------------------------------------------------------------------------------
	// References

	DynamicArray<VkAttachmentReference> targetRefs;
	CreateAttachmentReferences(0, m_targetImages, targetRefs);

	DynamicArray<VkAttachmentReference> targetInputRefs;
	CreateInputAttachmentReferences(0, m_targetImages, targetInputRefs);

	DynamicArray<VkAttachmentReference> gBufferRefs;
	CreateAttachmentReferences(m_targetImages.Count(), m_gBufferImages, gBufferRefs);

	DynamicArray<VkAttachmentReference> gBufferInputRefs;
	CreateInputAttachmentReferences(m_targetImages.Count(), m_gBufferImages, gBufferInputRefs);

	DynamicArray<VkAttachmentReference> depthRefs;
	CreateAttachmentReferences(m_targetImages.Count() + m_gBufferImages.Count(), depthImages, depthRefs);

	DynamicArray<VkAttachmentReference> depthInputRefs;
	CreateInputAttachmentReferences(m_targetImages.Count() + m_gBufferImages.Count(), depthImages, depthInputRefs);

	// ---------------------------------------------------------------------------------
	// Subpasses

	VkSubpassDescription gBufferSubpass = {};
	gBufferSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	gBufferSubpass.colorAttachmentCount = gBufferRefs.Count();
	gBufferSubpass.pColorAttachments = gBufferRefs.Data();
	gBufferSubpass.pDepthStencilAttachment = depthRefs.Data();

	VkSubpassDescription lightingSubpass = {};
	lightingSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	lightingSubpass.colorAttachmentCount = 1;
	lightingSubpass.pColorAttachments = &targetRefs[0];
	lightingSubpass.inputAttachmentCount = gBufferInputRefs.Count();
	lightingSubpass.pInputAttachments = gBufferInputRefs.Data();
	lightingSubpass.pDepthStencilAttachment = VK_NULL_HANDLE; // No depth needed for lighting.

	VkSubpassDescription subpasses[SUB_PASS_COUNT] = { gBufferSubpass, lightingSubpass };

	// ---------------------------------------------------------------------------------
	// Create Info

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = allDescriptions.Count();
	renderPassInfo.pAttachments = allDescriptions.Data();
	renderPassInfo.subpassCount = SUB_PASS_COUNT;
	renderPassInfo.pSubpasses = subpasses;

	// ---------------------------------------------------------------------------------
	// Subpass Dependencies

	const int nDependencyCount = 2;
	VkSubpassDependency dependencies[nDependencyCount] = { m_gBufferDependency, m_lightingDependency };

	renderPassInfo.dependencyCount = nDependencyCount;
	renderPassInfo.pDependencies = dependencies;

	// ---------------------------------------------------------------------------------
	// Create Render Pass

	RENDERER_SAFECALL(vkCreateRenderPass(m_renderer->GetDevice(), &renderPassInfo, nullptr, &m_pass), "Renderer Error: Failed to create render pass.");
}

inline void SubScene::CreateCmds()
{
	// ---------------------------------------------------------------------------------
	// Create command pool.

	VkCommandPoolCreateInfo cmdPoolCreateInfo = {};
	cmdPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	cmdPoolCreateInfo.queueFamilyIndex = m_nQueueFamilyIndex;
	cmdPoolCreateInfo.pNext = nullptr;

	RENDERER_SAFECALL(vkCreateCommandPool(m_renderer->GetDevice(), &cmdPoolCreateInfo, nullptr, &m_commandPool), "SubScene Error: Failed to create subscene command pool.");

	// ---------------------------------------------------------------------------------
	// Allocate command buffers.

	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = m_commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
	allocInfo.pNext = nullptr;

	m_primaryCmdBufs.SetSize(MAX_FRAMES_IN_FLIGHT);
	m_primaryCmdBufs.SetCount(MAX_FRAMES_IN_FLIGHT);

	RENDERER_SAFECALL(vkAllocateCommandBuffers(m_renderer->GetDevice(), &allocInfo, m_primaryCmdBufs.Data()), "Subscene Error: Failed to allocate primary command buffers.");
}

inline void SubScene::GetQueue()
{
	// Retreive device queue for rendering using provided queue family index.
	vkGetDeviceQueue(m_renderer->GetDevice(), m_nQueueFamilyIndex, 0, &m_renderQueue);
}

inline void SubScene::RecordPrimaryCmdBuffer(const uint32_t& nPresentImageIndex, const uint32_t& nFrameIndex)
{
	VkCommandBuffer cmdBuf = m_primaryCmdBufs[nPresentImageIndex];

	// Begin recording...
	RENDERER_SAFECALL(vkBeginCommandBuffer(cmdBuf, &m_primaryCmdBeginInfo), "Subscene Error: Failed to begin recording of primary command buffer.");

	// Iterate through all local render object pipelines and draw all member objects.

	// Finish recording.
	vkEndCommandBuffer(cmdBuf);
}
