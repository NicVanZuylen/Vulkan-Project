#include "SubScene.h"
#include "Renderer.h"
#include "GBufferPass.h"
#include "LightingManager.h"
#include "Material.h"
#include "Texture.h"
#include "RenderObject.h"

PipelineData::PipelineData()
{
	m_handle = nullptr;
	m_layout = nullptr;
	m_material = nullptr;
}

PipelineDataPtr::PipelineDataPtr()
{
	m_ptr = nullptr;
}

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
	VK_ATTACHMENT_STORE_OP_DONT_CARE,
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
	VK_ATTACHMENT_STORE_OP_DONT_CARE,
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
	VK_ATTACHMENT_STORE_OP_DONT_CARE,
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

SubScene::SubScene(SubSceneParams& params)
{
	m_renderer = params.m_renderer;

	m_gPass = new GBufferPass(m_renderer, &m_allPipelines, m_commandPool, m_framebuffers, m_pass, m_nQueueFamilyIndex);
	m_lightManager = new LightingManager(m_renderer, params.m_dirLightShader, params.m_pointLightShader, params.m_nFrameBufferWidth, params.m_nFrameBufferHeight);

	m_pass = VK_NULL_HANDLE;
	m_nQueueFamilyIndex = params.m_nQueueFamilyIndex;

	// Create output color image. HDR if specified.
	if(!params.m_bOutputHDR)
	    m_outImage = new Texture(m_renderer, params.m_nFrameBufferWidth, params.m_nFrameBufferHeight, ATTACHMENT_COLOR, VK_FORMAT_R8G8B8A8_UNORM, !params.m_bPrimary);
	else
		m_outImage = new Texture(m_renderer, params.m_nFrameBufferWidth, params.m_nFrameBufferHeight, ATTACHMENT_COLOR, VK_FORMAT_R16G16B16A16_SFLOAT, !params.m_bPrimary);

	// Output image clear value.
	VkClearValue clearVal = { 0.0f, 0.0f, 0.0f, 1.0f };
	m_clearValues.Push(clearVal);

	m_nWidth = params.m_nFrameBufferWidth;
	m_nHeight = params.m_nFrameBufferHeight;


	// Create G Buffer images.
	SetImages(params.eAttachmentBits);

	// Additional operations.
	CreateRenderPass(); // Create subscene render pass.
	CreateFramebuffers(); // Create subscene framebuffer.
	CreateCmds(); // Create command pool & primary command buffers.
	GetQueue(); // Get device queue subscene commands will be submitted to.
}

SubScene::~SubScene() 
{
	VkDevice device = m_renderer->GetDevice();

	// Destroy command pool.
	vkDestroyCommandPool(device, m_commandPool, nullptr);

	// Destroy framebuffers.
	for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	    vkDestroyFramebuffer(device, m_framebuffers[i], nullptr);

	// Destroy subscene render pass.
	vkDestroyRenderPass(device, m_pass, nullptr);

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) 
	{
		// Remove existing G Buffer images.
		for (uint32_t j = 0; j < m_gBufferImages[i].Count(); ++j)
			delete m_gBufferImages[i][j];

		if (m_depthImages[i])
			delete m_depthImages[i];

		m_colorImages[i] = nullptr;
		m_depthImages[i] = nullptr;
		m_posImages[i] = nullptr;
		m_normalImages[i] = nullptr;
	}

	delete m_gPass;
	delete m_lightManager;

	delete m_outImage;
}

void SubScene::SetImages(EGBufferAttachmentTypeBit eImageBits)
{
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		// Remove existing G Buffer images.
		for (uint32_t j = 0; j < m_gBufferImages[i].Count(); ++j)
			delete m_gBufferImages[i][j];

		m_gBufferImages[i].Clear();
		m_depthImages[i] = nullptr;


		// Create images the bit field contains...
		if (eImageBits & GBUFFER_COLOR_BIT)
		{
			// Create HDR image if correct bit is found.
			if (eImageBits & GBUFFER_COLOR_HDR_BIT)
				m_colorImages[i] = new Texture(m_renderer, m_nWidth, m_nHeight, ATTACHMENT_COLOR, VK_FORMAT_R16G16B16A16_SFLOAT, true);
			else
				m_colorImages[i] = new Texture(m_renderer, m_nWidth, m_nHeight, ATTACHMENT_COLOR, VK_FORMAT_R8G8B8A8_UNORM, true);

			m_gBufferImages[i].Push(m_colorImages[i]);

			if (i == 0)
			{
			    VkClearValue clearVal = { 0.0f, 0.0f, 0.0f, 1.0f };
			    m_clearValues.Push(clearVal);
			}
		}

		if (eImageBits & GBUFFER_DEPTH_BIT)
		{
			// Specify pool of depth formats and find the best available format.
			DynamicArray<VkFormat> depthFormats = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
			VkFormat depthFormat = RendererHelper::FindBestDepthFormat(m_renderer->GetPhysDevice(), depthFormats, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

			m_depthImages[i] = new Texture(m_renderer, m_nWidth, m_nHeight, ATTACHMENT_DEPTH_STENCIL, depthFormat);

			if (i == 0)
			{
			    VkClearValue clearVal = { 1.0f, 1.0f, 1.0f, 1.0f };
			    m_clearValues.Push(clearVal);
			}
		}

		if (eImageBits & GBUFFER_POSITION_BIT)
		{
			m_posImages[i] = new Texture(m_renderer, m_nWidth, m_nHeight, ATTACHMENT_COLOR, VK_FORMAT_R16G16B16A16_SFLOAT, true);
			m_gBufferImages[i].Push(m_posImages[i]);

			if (i == 0)
			{
			    VkClearValue clearVal = { 0.0f, 0.0f, 0.0f, 1.0f };
			    m_clearValues.Push(clearVal);
			}
		}

		if (eImageBits & GBUFFER_NORMAL_BIT)
		{
			m_normalImages[i] = new Texture(m_renderer, m_nWidth, m_nHeight, ATTACHMENT_COLOR, VK_FORMAT_R16G16B16A16_SFLOAT, true);
			m_gBufferImages[i].Push(m_normalImages[i]);

			if (i == 0) 
			{
			    VkClearValue clearVal = { 0.0f, 0.0f, 0.0f, 1.0f };
			    m_clearValues.Push(clearVal);
			}
		}

		// Update images array.
		m_allImages[i].Clear();
		m_allImages[i].Push(m_outImage);
		m_allImages[i] += m_gBufferImages[i];

		if (m_depthImages[i])
			m_allImages[i].Push(m_depthImages[i]);
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
	vkQueueSubmit(m_renderQueue, 1, &m_renderSubmitInfo, signalFence);
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

		// Set format if different from default.
		attachments[i].format = targets[i]->Format();
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
			RENDERER_SAFECALL(true, "SubScene Error: Invalid Enum!");
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

	DynamicArray<Texture*> targetImages = { m_outImage };

	DynamicArray<VkAttachmentDescription> targetDescriptions;
	CreateAttachmentDescriptions(targetImages, targetDescriptions);

	DynamicArray<VkAttachmentDescription> gDescriptions;
	CreateAttachmentDescriptions(m_gBufferImages[0], gDescriptions);

	DynamicArray<Texture*> depthImages = { m_depthImages[0] };

	DynamicArray<VkAttachmentDescription> depthDescriptions;
	CreateAttachmentDescriptions(depthImages, depthDescriptions);

	DynamicArray<VkAttachmentDescription> allDescriptions = targetDescriptions + gDescriptions + depthDescriptions;

	// ---------------------------------------------------------------------------------
	// References

	DynamicArray<VkAttachmentReference> targetRefs;
	CreateAttachmentReferences(0, targetImages, targetRefs);

	DynamicArray<VkAttachmentReference> targetInputRefs;
	CreateInputAttachmentReferences(0, targetImages, targetInputRefs);

	DynamicArray<VkAttachmentReference> gBufferRefs;
	CreateAttachmentReferences(targetImages.Count(), m_gBufferImages[0], gBufferRefs);

	DynamicArray<VkAttachmentReference> gBufferInputRefs;
	CreateInputAttachmentReferences(targetImages.Count(), m_gBufferImages[0], gBufferInputRefs);

	DynamicArray<VkAttachmentReference> depthRefs;
	CreateAttachmentReferences(targetImages.Count() + m_gBufferImages[0].Count(), depthImages, depthRefs);

	DynamicArray<VkAttachmentReference> depthInputRefs;
	CreateInputAttachmentReferences(targetImages.Count() + m_gBufferImages[0].Count(), depthImages, depthInputRefs);

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

inline void SubScene::CreateFramebuffers()
{
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) 
	{
	    // Destroy previous framebuffers if they exist.
		if(m_framebuffers[i])
		    vkDestroyFramebuffer(m_renderer->GetDevice(), m_framebuffers[i], nullptr);

		// Gather all images and extract their image views into another array.
		DynamicArray<VkImageView> views(m_allImages[i].Count());

		for (int j = 0; j < m_allImages[i].Count(); ++j)
		{
			views.Push(m_allImages[i][j]->ImageView());
		}

		VkFramebufferCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		createInfo.attachmentCount = views.Count();
		createInfo.pAttachments = views.Data();
		createInfo.renderPass = m_pass;
		createInfo.layers = 1;
		createInfo.width = m_nWidth;
		createInfo.height = m_nHeight;
		createInfo.flags = 0;
		createInfo.pNext = nullptr;

		RENDERER_SAFECALL(vkCreateFramebuffer(m_renderer->GetDevice(), &createInfo, nullptr, &m_framebuffers[i]), "SubScene Error: Failed to create framebuffer.");
	}
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
	VkCommandBuffer cmdBuf = m_primaryCmdBufs[nFrameIndex];

	// Begin recording...
	RENDERER_SAFECALL(vkBeginCommandBuffer(cmdBuf, &m_primaryCmdBeginInfo), "Subscene Error: Failed to begin recording of primary command buffer.");

	VkRect2D renderArea = { 0, 0, m_nWidth, m_nHeight };

	VkRenderPassBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	beginInfo.framebuffer = m_framebuffers[nPresentImageIndex];
	beginInfo.clearValueCount = m_clearValues.Count();
	beginInfo.pClearValues = m_clearValues.Data();
	beginInfo.renderArea = renderArea;
	beginInfo.renderPass = m_pass;
	beginInfo.pNext = nullptr;

	// Begin render pass instance.
	vkCmdBeginRenderPass(cmdBuf, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);

	// G-Buffer Pass
	vkCmdExecuteCommands(cmdBuf, 1, m_gPass->GetCommandBuffer(nFrameIndex));

	// Execute lighting pass.


	// End render pass instance.
	vkCmdEndRenderPass(cmdBuf);

	// Finish recording.
	RENDERER_SAFECALL(vkEndCommandBuffer(cmdBuf), "SubScene Error: Failed to end recording of dynamic object command buffer.");
}
