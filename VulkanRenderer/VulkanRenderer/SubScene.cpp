#include "SubScene.h"
#include "Renderer.h"
#include "GBufferPass.h"
#include "LightingManager.h"
#include "Material.h"
#include "Texture.h"
#include "RenderObject.h"
#include "gtc/matrix_transform.hpp"

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

VkDescriptorPoolCreateInfo SubScene::m_poolCreateInfo = 
{
	VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
	nullptr,
	0,
	0,
	1,
	nullptr
};

VkDescriptorSetLayoutBinding SubScene::m_mvpUBOLayoutBinding =
{
	0,
	VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	1,
	VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
	nullptr
};

VkDescriptorSetLayoutCreateInfo SubScene::m_mvpSetLayoutInfo = 
{
	VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
	nullptr,
	0,
	1,
	&SubScene::m_mvpUBOLayoutBinding
};

VkDescriptorSetLayoutBinding SubScene::m_inAttachLayoutBinding =
{
	0,
	VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
	1,
	VK_SHADER_STAGE_FRAGMENT_BIT,
	nullptr
};

VkDescriptorSetLayoutCreateInfo SubScene::m_inAttachSetLayoutInfo =
{
	VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
	nullptr,
	0,
	1,
	&SubScene::m_inAttachLayoutBinding
};

VkDescriptorSetAllocateInfo SubScene::m_descAllocInfo =
{
	VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
	nullptr,
	VK_NULL_HANDLE,
	1,
	nullptr
};

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

	m_pass = VK_NULL_HANDLE;
	m_nQueueFamilyIndex = params.m_nQueueFamilyIndex;

	// Output image clear value.
	VkClearValue clearVal = { 0.0f, 0.0f, 0.0f, 1.0f };
	m_clearValues.Push(clearVal);

	m_nWidth = params.m_nFrameBufferWidth;
	m_nHeight = params.m_nFrameBufferHeight;
	m_bPrimary = params.m_bPrimary;
	m_bOutputHDR = params.m_bOutputHDR;

	// Create images that will be rendered to.
	CreateImages(params.eAttachmentBits, params.m_miscGAttachments);

	// Create MVP UBO buffers
	CreateMVPUBOBuffers();

	// Create descriptor pool & sets
	CreateDescriptorPool();
	CreateMVPUBODescriptors(); // MVP UBO descriptors need to be created before the lighting pass render module.
	CreateInputAttachmentDescriptors(); // Create descriptor sets for G Buffer input attachments & subscene output input attachments.

	// Update all descriptor sets.
	UpdateAllDescriptorSets();

	// Additional constructions

	CreateRenderPass(); // Create subscene render pass.
	CreateFramebuffers(); // Create subscene framebuffer.
	CreateCmds(); // Create command pool & primary command buffers.
	GetQueue(); // Get device queue subscene commands will be submitted to.

	// Modules

	m_gPass = new GBufferPass(m_renderer, &m_allPipelines, m_commandPool, m_pass, m_mvpUBODescSets, m_nQueueFamilyIndex);
	m_lightManager = new LightingManager
	(
		m_renderer,
		params.m_dirLightShader,
		params.m_pointLightShader,
		m_mvpUBODescSets,
		m_gBufferDescSet,
		params.m_nFrameBufferWidth,
		params.m_nFrameBufferHeight,
		m_commandPool,
		m_pass,
		m_mvpUBOSetLayout,
		m_gBufferSetLayout,
		m_nQueueFamilyIndex
	);
}

SubScene::~SubScene() 
{
	VkDevice device = m_renderer->GetDevice();

	// Wait for any rendering to finish before destroying.
	vkDeviceWaitIdle(device);

	// ---------------------------------------------------------------------------------
	// Destroy descriptors.

	vkDestroyDescriptorPool(device, m_descPool, nullptr);
	vkDestroyDescriptorSetLayout(device, m_mvpUBOSetLayout, nullptr);

	// Destroy input attachment set layout.
	vkDestroyDescriptorSetLayout(device, m_gBufferSetLayout, nullptr);

	// ---------------------------------------------------------------------------------
	// Destroy command pool.
	vkDestroyCommandPool(device, m_commandPool, nullptr);

	// ---------------------------------------------------------------------------------
	// Destroy framebuffers.

	for(uint32_t i = 0; i < m_framebuffers.Count(); ++i)
	    vkDestroyFramebuffer(device, m_framebuffers[i], nullptr);

	// ---------------------------------------------------------------------------------
	// Destroy subscene render pass.
	vkDestroyRenderPass(device, m_pass, nullptr);

	// ---------------------------------------------------------------------------------
	// Destroy MVP UBO Buffers

	for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) 
	{
		vkDestroyBuffer(device, m_mvpUBOBuffers[i], nullptr);
		vkFreeMemory(device, m_mvpUBOMemories[i], nullptr);
	}

	// ---------------------------------------------------------------------------------
	// Destroy G Buffer images.

	for (uint32_t i = 0; i < m_gBufferImages.Count(); ++i)
		delete m_gBufferImages[i];

	if (m_depthImage)
		delete m_depthImage;

	m_colorImage = nullptr;
	m_depthImage = nullptr;
	m_posImage = nullptr;
	m_normalImage = nullptr;

	delete m_gPass;
	delete m_lightManager;

	if(m_bPrimary)
	    delete m_outImage;
}

void SubScene::CreateImages(EGBufferAttachmentTypeBit eImageBits, const DynamicArray<MiscGBufferDesc>& miscGAttachments)
{
	// Remove existing images.
	for(int i = 0; i < m_allImages.Count(); ++i) 
		delete m_allImages[i];

	CreateOutputImage();

	m_gBufferImages.Clear();
	m_gBufferImages.SetSize(3 + miscGAttachments.Count());
	m_depthImage = nullptr;

	m_clearValues.Clear();

	// Push output clear value.
	m_clearValues.Push({ 0.0f, 0.0f, 0.0f, 1.0f });

	// Create images the bit field contains...
	if (eImageBits & GBUFFER_COLOR_BIT)
	{
		// Create HDR image if correct bit is found.
		if (eImageBits & GBUFFER_COLOR_HDR_BIT)
			m_colorImage = new Texture(m_renderer, m_nWidth, m_nHeight, ATTACHMENT_COLOR, VK_FORMAT_R16G16B16A16_SFLOAT, true);
		else
			m_colorImage = new Texture(m_renderer, m_nWidth, m_nHeight, ATTACHMENT_COLOR, VK_FORMAT_R8G8B8A8_UNORM, true);

		m_gBufferImages.Push(m_colorImage);

		// Clear value
		VkClearValue clearVal = { 0.0f, 0.0f, 0.0f, 1.0f };
		m_clearValues.Push(clearVal);
	}

	if (eImageBits & GBUFFER_POSITION_BIT)
	{
		m_posImage = new Texture(m_renderer, m_nWidth, m_nHeight, ATTACHMENT_COLOR, VK_FORMAT_R16G16B16A16_SFLOAT, true);
		m_gBufferImages.Push(m_posImage);

		// Clear value
		VkClearValue clearVal = { 0.0f, 0.0f, 0.0f, 1.0f };
		m_clearValues.Push(clearVal);
	}

	if (eImageBits & GBUFFER_NORMAL_BIT)
	{
		m_normalImage = new Texture(m_renderer, m_nWidth, m_nHeight, ATTACHMENT_COLOR, VK_FORMAT_R16G16B16A16_SFLOAT, true);
		m_gBufferImages.Push(m_normalImage);

		// Clear value
		VkClearValue clearVal = { 0.0f, 0.0f, 0.0f, 1.0f };
		m_clearValues.Push(clearVal);
	}

	m_miscGAttachments = miscGAttachments;

	// Create misc G buffer images...
	for (uint32_t i = 0; i < m_miscGAttachments.Count(); ++i)
	{
		Texture* miscTex = nullptr;

		// Create image with provided format.
		switch (m_miscGAttachments[i].m_eType)
		{
		case EMiscGBufferType::GBUFFER_MISC_8_BIT:
			miscTex = new Texture(m_renderer, m_nWidth, m_nHeight, ATTACHMENT_COLOR, VK_FORMAT_R8G8B8A8_UNORM, true);
			break;

		case EMiscGBufferType::GBUFFER_MISC_16_BIT_FLOAT:
			miscTex = new Texture(m_renderer, m_nWidth, m_nHeight, ATTACHMENT_COLOR, VK_FORMAT_R16G16B16A16_SFLOAT, true);
			break;

		case EMiscGBufferType::GBUFFER_MISC_32_BIT_FLOAT:
			miscTex = new Texture(m_renderer, m_nWidth, m_nHeight, ATTACHMENT_COLOR, VK_FORMAT_R32G32B32_SFLOAT, true);
			break;

		default:

			break;
		}

		// Add new image to G buffer image array.
		if (miscTex)
		{
			m_gBufferImages.Push(miscTex);

			glm::vec4 v4ClearColor = m_miscGAttachments[i].m_v4ClearColor;

			// Add corresponding clear value.
			m_clearValues.Push({ v4ClearColor.x, v4ClearColor.y, v4ClearColor.z, v4ClearColor.w });
		}
	}

	if (eImageBits & GBUFFER_DEPTH_BIT)
	{
		// Specify pool of depth formats and find the best available format.
		DynamicArray<VkFormat> depthFormats = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
		VkFormat depthFormat = RendererHelper::FindBestDepthFormat(m_renderer->GetPhysDevice(), depthFormats, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

		m_depthImage = new Texture(m_renderer, m_nWidth, m_nHeight, ATTACHMENT_DEPTH_STENCIL, depthFormat);

		// Clear value
		VkClearValue clearVal = { 1.0f, 1.0f, 1.0f, 1.0f };
		m_clearValues.Push(clearVal);
	}

	// Update images array.
	m_allImages.Clear();
	m_allImages.SetSize(2 + m_gBufferImages.Count());

	if(!m_bPrimary)
	    m_allImages.Push(m_outImage);

	m_allImages += m_gBufferImages;

	if (m_depthImage)
		m_allImages.Push(m_depthImage);

	// Store image bit field information.
	m_eGBufferImageBits = eImageBits;
}

void SubScene::ResizeOutput(uint32_t nNewWidth, uint32_t nNewHeight)
{
	m_nWidth = nNewWidth;
	m_nHeight = nNewHeight;

	VkDevice device = m_renderer->GetDevice();

	// ---------------------------------------------------------------------------------
	// Free descriptor sets.

	vkResetDescriptorPool(device, m_descPool, 0);

	// ---------------------------------------------------------------------------------
	// Re-creation

	CreateOutputImage();
	CreateImages(m_eGBufferImageBits, m_miscGAttachments); // Re-create subscene image.
	CreateRenderPass(); // Re-create render pass.
	CreateFramebuffers(); // Re-create framebuffers.
	CreateMVPUBODescriptors(false); // Re-create descriptor sets for the MVP UBO buffers.
	CreateInputAttachmentDescriptors(false); // Re-create descriptor sets for G Buffer & ouput input attachments.
	UpdateAllDescriptorSets(); // Update descriptors in the sets.

	// Re-create pipelines.
	for (int i = 0; i < m_allPipelines.Count(); ++i)
		m_allPipelines[i]->m_renderObjects[0]->RecreatePipeline();

	// Have modules re-create resources if necessary & give them the updated descriptor sets.
	RenderModuleResizeData resizeData;
	resizeData.m_nWidth = m_nWidth;
	resizeData.m_nHeight = m_nHeight;
	resizeData.m_mvpUBOSets = m_mvpUBODescSets;
	resizeData.m_gBufferSets = &m_gBufferDescSet;
	resizeData.m_renderPass = m_pass;

	m_gPass->OnOutputResize(resizeData);
	m_lightManager->OnOutputResize(resizeData);
}

void SubScene::UpdateCameraView(const glm::mat4& view, const glm::vec4& v4ViewPos)
{
	m_localMVPData.m_view = view;
	m_localMVPData.m_v4ViewPos = v4ViewPos;
}

void SubScene::AddPipeline(PipelineData* pipeline)
{
	m_allPipelines.Push(pipeline);
}

VkCommandBuffer& SubScene::GetCommandBuffer(const uint32_t nIndex) 
{
	return m_primaryCmdBufs[nIndex];
}

const VkRenderPass& SubScene::GetRenderPass()
{
	return m_pass;
}

VkDescriptorSetLayout SubScene::MVPUBOLayout() 
{
	return m_mvpUBOSetLayout;
}

Table<PipelineDataPtr>& SubScene::GetPipelineTable()
{
	return m_pipelines;
}

GBufferPass* SubScene::GetGBufferPass()
{
	return m_gPass;
}

const uint32_t& SubScene::GetGBufferCount() 
{
	return m_gBufferImages.Count();
}

LightingManager* SubScene::GetLightingManager()
{
	return m_lightManager;
}

Renderer* SubScene::GetRenderer()
{
	return m_renderer;
}

inline void SubScene::CreateOutputImage()
{
	if (m_bPrimary) 
	{
		m_swapchainImageViews = m_renderer->SwapChainImageViews(); // Set swap chain image view references.
		return;
	}

	if (m_bOutputHDR) // Create a HDR image if specified.
		m_outImage = new Texture(m_renderer, m_nWidth, m_nHeight, ATTACHMENT_COLOR, VK_FORMAT_R8G8B8A8_UNORM, true);
	else // Otherwise create a standard output image.
		m_outImage = new Texture(m_renderer, m_nWidth, m_nHeight, ATTACHMENT_COLOR, VK_FORMAT_R16G16B16A16_SFLOAT, true);
}

inline void SubScene::CreateDescriptorPool()
{
	// ---------------------------------------------------------------------------------
    // Descriptor Pool

    // Pool size for MVP UBO buffers.
	VkDescriptorPoolSize mvpUBOPoolSize = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT };

	// Pool size for G Buffer input attachment set
	VkDescriptorPoolSize gBufferPoolSize = { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, m_gBufferImages.Count() };

	VkDescriptorPoolSize outputPoolSize = { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1 };

	VkDescriptorPoolSize poolSizes[3] = { mvpUBOPoolSize, gBufferPoolSize, outputPoolSize };

	// Set pool create info poolsizes.
	m_poolCreateInfo.maxSets = MAX_FRAMES_IN_FLIGHT + m_gBufferImages.Count() + !m_bPrimary; // Sets for: MVP UBO Buffers, G Buffer Images, Output image
	m_poolCreateInfo.poolSizeCount = 2 + !m_bPrimary;
	m_poolCreateInfo.pPoolSizes = poolSizes;

	// Create descriptor pool.
	RENDERER_SAFECALL(vkCreateDescriptorPool(m_renderer->GetDevice(), &m_poolCreateInfo, nullptr, &m_descPool), "SubScene Error: Failed to create MVP UBO descriptor pool.");
}

inline void SubScene::CreateMVPUBODescriptors(bool bCreateLayout)
{
	// ---------------------------------------------------------------------------------
	// MVP UBO Set layout

	if (bCreateLayout)
	    RENDERER_SAFECALL(vkCreateDescriptorSetLayout(m_renderer->GetDevice(), &m_mvpSetLayoutInfo, nullptr, &m_mvpUBOSetLayout), "SubScene Error: Failed to create MVP UBO descriptor set layout.");

	// ---------------------------------------------------------------------------------
	// Descriptor set allocation.

	DynamicArray<VkDescriptorSetLayout> setLayouts(MAX_FRAMES_IN_FLIGHT, 1);
	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		setLayouts.Push(m_mvpUBOSetLayout);

	m_descAllocInfo.descriptorPool = m_descPool;
	m_descAllocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
	m_descAllocInfo.pSetLayouts = setLayouts.Data();

	// Allocate descriptor sets...
	RENDERER_SAFECALL(vkAllocateDescriptorSets(m_renderer->GetDevice(), &m_descAllocInfo, m_mvpUBODescSets), "SubScene Error: Failed to allocate MVP UBO descriptor sets.");
}

inline void SubScene::CreateInputAttachmentDescriptors(bool bCreateLayout)
{
	// ---------------------------------------------------------------------------------
	// Set layouts

	if(bCreateLayout) 
	{
		m_inAttachLayoutBinding.descriptorCount = m_gBufferImages.Count();

		// G Buffer set layout
		RENDERER_SAFECALL(vkCreateDescriptorSetLayout(m_renderer->GetDevice(), &m_inAttachSetLayoutInfo, nullptr, &m_gBufferSetLayout), "SubScene Error: Failed to create G Buffer descriptor set layout.");

		m_inAttachLayoutBinding.descriptorCount = 1;

		// Output set layout
		if (!m_bPrimary)
			RENDERER_SAFECALL(vkCreateDescriptorSetLayout(m_renderer->GetDevice(), &m_inAttachSetLayoutInfo, nullptr, &m_outputSetLayout), "SubScene Error: Failed to create G Buffer descriptor set layout.");
	}

	// ---------------------------------------------------------------------------------
	// Descriptor set allocation.

	m_descAllocInfo.descriptorPool = m_descPool;
	m_descAllocInfo.descriptorSetCount = 1;
	m_descAllocInfo.pSetLayouts = &m_gBufferSetLayout;

	// Set descriptor count for G buffer images.
	//m_inAttachLayoutBinding.descriptorCount = m_gBufferImages.Count();

	RENDERER_SAFECALL(vkAllocateDescriptorSets(m_renderer->GetDevice(), &m_descAllocInfo, &m_gBufferDescSet), "SubScene Error: Failed to allocate G Buffer descriptor sets.");

	// One descriptor for output.
	m_descAllocInfo.pSetLayouts = &m_outputSetLayout;

	//m_inAttachLayoutBinding.descriptorCount = 1;

	if(!m_bPrimary) 
	{
		RENDERER_SAFECALL(vkAllocateDescriptorSets(m_renderer->GetDevice(), &m_descAllocInfo, &m_outputDescSet), "SubScene Error: Failed to allocate G Buffer descriptor sets.");
	}
}

inline void SubScene::UpdateAllDescriptorSets()
{
	// ---------------------------------------------------------------------------------
	// Write structures

	VkWriteDescriptorSet writes[MAX_FRAMES_IN_FLIGHT + 2];

	// ---------------------------------------------------------------------------------
	// MVP UBO Updates

	VkDescriptorBufferInfo mvpUBOBufferInfos[MAX_FRAMES_IN_FLIGHT];

	// Set buffer infos for each MVP UBO buffer.
	for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) 
	{
		mvpUBOBufferInfos[i].buffer = m_mvpUBOBuffers[i];
		mvpUBOBufferInfos[i].offset = 0;
		mvpUBOBufferInfos[i].range = VK_WHOLE_SIZE;

		VkWriteDescriptorSet mvpUBOWrite = {};
		mvpUBOWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		mvpUBOWrite.dstSet = m_mvpUBODescSets[i];
		mvpUBOWrite.descriptorCount = 1;
		mvpUBOWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		mvpUBOWrite.dstArrayElement = 0;
		mvpUBOWrite.pBufferInfo = &mvpUBOBufferInfos[i];
		mvpUBOWrite.pNext = nullptr;

		writes[i] = mvpUBOWrite;
	}

	// ---------------------------------------------------------------------------------
	// G-Buffer Updates

	VkWriteDescriptorSet gBufferWrite = {};
	gBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	gBufferWrite.dstSet = m_gBufferDescSet;
	gBufferWrite.descriptorCount = m_gBufferImages.Count();
	gBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	gBufferWrite.dstArrayElement = 0;
	gBufferWrite.pNext = nullptr;

	DynamicArray<VkDescriptorImageInfo> allImageInfos(m_gBufferImages.Count(), 1);

	VkDescriptorImageInfo currentImageInfo = {};
	currentImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	currentImageInfo.sampler = VK_NULL_HANDLE; // Input attachments do not need samplers.

	// Add an image info for each G buffer image.
	for (uint32_t i = 0; i < m_gBufferImages.Count(); ++i)
	{
		currentImageInfo.imageView = m_gBufferImages[i]->ImageView();
		allImageInfos.Push(currentImageInfo);
	}

	// Set image infos.
	gBufferWrite.pImageInfo = allImageInfos.Data();

	writes[MAX_FRAMES_IN_FLIGHT] = gBufferWrite;

	// ---------------------------------------------------------------------------------
	// Output Updates

	if(!m_bPrimary) 
	{
		currentImageInfo.imageView = m_outImage->ImageView();

		VkWriteDescriptorSet outputWrite = {};
		outputWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		outputWrite.dstSet = m_outputDescSet;
		outputWrite.descriptorCount = 1;
		outputWrite.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		outputWrite.dstArrayElement = 0;
		outputWrite.pImageInfo = &currentImageInfo;
		outputWrite.pNext = nullptr;

		writes[MAX_FRAMES_IN_FLIGHT + 1] = outputWrite;
	}

	// ---------------------------------------------------------------------------------
	// Update descriptors.

	vkUpdateDescriptorSets(m_renderer->GetDevice(), MAX_FRAMES_IN_FLIGHT + 1 + !m_bPrimary, writes, 0, nullptr);
}

inline void SubScene::CreateMVPUBOBuffers()
{
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		m_renderer->CreateBuffer(sizeof(MVPUniformBuffer), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
			m_mvpUBOBuffers[i], m_mvpUBOMemories[i]);
}

inline void SubScene::CreateSwapChainAttachmentDesc(DynamicArray<VkAttachmentDescription>& attachments)
{
	attachments.SetSize(1);
	attachments.Clear();

	m_swapChainAttachmentDescription.format = m_renderer->SwapChainImageFormat();

	attachments.Push(m_swapChainAttachmentDescription);
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

inline void SubScene::CreateSwapChainAttachRef(DynamicArray<VkAttachmentReference>& references)
{
	references.SetSize(1);
	references.Clear();

	VkAttachmentReference swapChainRef = {};
	swapChainRef.attachment = 0;
	swapChainRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	references.Push(swapChainRef);
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

	// Create attachment descriptions for output images, or swap chain images if primary.
	if (!m_bPrimary)
		CreateAttachmentDescriptions(targetImages, targetDescriptions);
	else
		CreateSwapChainAttachmentDesc(targetDescriptions);

	DynamicArray<VkAttachmentDescription> gDescriptions;
	CreateAttachmentDescriptions(m_gBufferImages, gDescriptions);

	DynamicArray<Texture*> depthImages = { m_depthImage };

	DynamicArray<VkAttachmentDescription> depthDescriptions;
	CreateAttachmentDescriptions(depthImages, depthDescriptions);

	DynamicArray<VkAttachmentDescription> allDescriptions = targetDescriptions + gDescriptions + depthDescriptions;

	// ---------------------------------------------------------------------------------
	// References

	DynamicArray<VkAttachmentReference> targetRefs;
	DynamicArray<VkAttachmentReference> targetInputRefs;

	// Create attachment references for output images. Or swap chain images if primary.
	if(!m_bPrimary) 
	{
		CreateAttachmentReferences(0, targetImages, targetRefs);
		CreateInputAttachmentReferences(0, targetImages, targetInputRefs);
	}
	else 
	{
		CreateSwapChainAttachRef(targetRefs);
	}

	DynamicArray<VkAttachmentReference> gBufferRefs;
	CreateAttachmentReferences(targetImages.Count(), m_gBufferImages, gBufferRefs);

	DynamicArray<VkAttachmentReference> gBufferInputRefs;
	CreateInputAttachmentReferences(targetImages.Count(), m_gBufferImages, gBufferInputRefs);

	DynamicArray<VkAttachmentReference> depthRefs;
	CreateAttachmentReferences(targetImages.Count() + m_gBufferImages.Count(), depthImages, depthRefs);

	DynamicArray<VkAttachmentReference> depthInputRefs;
	CreateInputAttachmentReferences(targetImages.Count() + m_gBufferImages.Count(), depthImages, depthInputRefs);

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
	// Destroy previous framebuffers if they exist.
	for (uint32_t i = 0; i < m_framebuffers.Count(); ++i) 
	{
		if (m_framebuffers[i]) 
		{
	        vkDestroyFramebuffer(m_renderer->GetDevice(), m_framebuffers[i], nullptr);
			m_framebuffers[i] = nullptr;
		}
	}

	m_framebuffers.Clear();

	uint32_t nFrameBufferCount = 1;

	// If this is a primary subscene we need a framebuffer for each swap chain image.
	if (m_bPrimary)
		nFrameBufferCount = m_swapchainImageViews.Count();

	// Make room for enough framebuffer handles.
	m_framebuffers.SetSize(nFrameBufferCount);

	for(uint32_t i = 0; i < nFrameBufferCount; ++i) 
	{
		// Gather all images and extract their image views into another array.
		DynamicArray<VkImageView> views(m_allImages.Count());

		// Push swap chain image (if primary).
		if (m_bPrimary)
			views.Push(m_swapchainImageViews[i]);

		// If primary, start at index 1 to skip the output image.
		for (uint32_t i = 0; i < m_allImages.Count(); ++i)
		{
			views.Push(m_allImages[i]->ImageView());
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

		m_framebuffers.Push(m_framebuffers[i]);
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

void SubScene::RecordPrimaryCmdBuffer(const uint32_t& nPresentImageIndex, const uint32_t& nFrameIndex, VkCommandBuffer& transferCmdBuf)
{
	VkCommandBuffer cmdBuf = m_primaryCmdBufs[nFrameIndex];

	// Begin recording...
	RENDERER_SAFECALL(vkBeginCommandBuffer(cmdBuf, &m_primaryCmdBeginInfo), "Subscene Error: Failed to begin recording of primary command buffer.");

	VkRect2D renderArea = { 0, 0, m_nWidth, m_nHeight };

	VkRenderPassBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	beginInfo.clearValueCount = m_clearValues.Count();
	beginInfo.pClearValues = m_clearValues.Data();
	beginInfo.renderArea = renderArea;
	beginInfo.renderPass = m_pass;
	beginInfo.pNext = nullptr;

	// If this is a primary subscene we will render directly to a swap chain image. The correct one is in the framebuffer indexed with the present image index.
	if (m_bPrimary)
		beginInfo.framebuffer = m_framebuffers[nPresentImageIndex];
	else
		beginInfo.framebuffer = m_framebuffers[0]; // There will be only one framebuffer if this is not a primary subscene.

	// Update MVP UBO
	UpdateMVPUBO(nFrameIndex);

	// Begin render pass instance.
	vkCmdBeginRenderPass(cmdBuf, &beginInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

	// G-Buffer Pass
	m_gPass->RecordCommandBuffer(nPresentImageIndex, nFrameIndex, beginInfo.framebuffer, transferCmdBuf);
	vkCmdExecuteCommands(cmdBuf, 1, m_gPass->GetCommandBuffer(nFrameIndex));

	// Next subpass.
	vkCmdNextSubpass(cmdBuf, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

	// Execute lighting pass.
	m_lightManager->RecordCommandBuffer(nPresentImageIndex, nFrameIndex, beginInfo.framebuffer, transferCmdBuf);
	vkCmdExecuteCommands(cmdBuf, 1, m_lightManager->GetCommandBuffer(nFrameIndex));

	// End render pass instance.
	vkCmdEndRenderPass(cmdBuf);

	// Finish recording.
	RENDERER_SAFECALL(vkEndCommandBuffer(cmdBuf), "SubScene Error: Failed to end recording of dynamic object command buffer.");
}

inline void SubScene::UpdateMVPUBO(const uint32_t& nFrameIndex)
{
	// Change coordinate system using this matrix.
	glm::mat4 axisCorrection; // Inverts the Y axis to match the OpenGL coordinate system.
	axisCorrection[1][1] = -1.0f;
	axisCorrection[2][2] = 1.0f;
	axisCorrection[3][3] = 1.0f;

	// Update local projection matrix.
	m_localMVPData.m_proj = axisCorrection * glm::perspective(45.0f, static_cast<float>(m_nWidth) / static_cast<float>(m_nHeight), 0.1f, 1000.0f);

	uint32_t nBufferSize = sizeof(MVPUniformBuffer);

	// Map & Update buffer contents...
	void* buffer = nullptr;
	vkMapMemory(m_renderer->GetDevice(), m_mvpUBOMemories[nFrameIndex], 0, nBufferSize, 0, &buffer);

	// Copy...
	std::memcpy(buffer, &m_localMVPData, nBufferSize);

	vkUnmapMemory(m_renderer->GetDevice(), m_mvpUBOMemories[nFrameIndex]);
}
