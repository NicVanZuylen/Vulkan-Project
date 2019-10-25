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
	VK_ATTACHMENT_LOAD_OP_DONT_CARE,
	VK_ATTACHMENT_STORE_OP_DONT_CARE,
	VK_ATTACHMENT_LOAD_OP_LOAD,
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

SubScene::SubScene(Renderer* renderer, unsigned int nQueueFamilyIndex, unsigned int nOutWidth, unsigned int nOutHeight, bool bPrimary)
{
	m_renderer = renderer;

	m_gPass = new GBufferPass(m_renderer, nQueueFamilyIndex, false);
	m_lightManager = new LightingManager(m_renderer, nullptr, nullptr, nOutWidth, nOutHeight);

	m_outImage = new Texture(m_renderer, nOutWidth, nOutHeight, ATTACHMENT_COLOR, VK_FORMAT_R8G8B8A8_UNORM, !bPrimary);
}

SubScene::~SubScene() 
{
	// Destroy subscene render pass.
	vkDestroyRenderPass(m_renderer->GetDevice(), m_pass, nullptr);

	m_colorImage = nullptr;
	m_depthImage = nullptr;
	m_posImage = nullptr;
	m_normalImage = nullptr;

	delete m_gPass;
	delete m_lightManager;

	delete m_outImage;
}

void SubScene::SetImages(Texture* colorImage, Texture* depthImage, Texture* posImage, Texture* normalImage)
{
	m_colorImage = colorImage;
	m_depthImage = depthImage;
	m_posImage = posImage;
	m_normalImage = normalImage;

	m_gBufferImages = { m_colorImage, m_posImage, m_depthImage };
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
	attachments.SetSize(targets.Count());
	attachments.SetCount(attachments.GetSize());

	for (int i = 0; i < targets.Count(); ++i)
	{
		switch (targets[i]->GetAttachmentType)
		{
		case ATTACHMENT_COLOR:
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

	for (int i = 0; i < targets.Count(); ++i)
	{
		ref.attachment = nIndexOffset + i;

		switch (targets[i]->GetAttachmentType)
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

	for (int i = 0; i < targets.Count(); ++i)
	{
		references[i] = ref;
		++ref.attachment;
	}
}

inline void SubScene::CreateRenderPass()
{
	// ---------------------------------------------------------------------------------
	// Descriptions

	m_targetImages = { m_outImage };

	DynamicArray<VkAttachmentDescription> targetDescriptions;
	CreateAttachmentDescriptions(m_targetImages, targetDescriptions);

	DynamicArray<VkAttachmentDescription> gDescriptions;
	CreateAttachmentDescriptions(m_gBufferImages, gDescriptions);

	DynamicArray<VkAttachmentDescription> allDescriptions = targetDescriptions + gDescriptions;

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

	DynamicArray<Texture*> depthImages = { m_depthImage };

	DynamicArray<VkAttachmentReference> depthRefs;
	CreateAttachmentReferences(m_targetImages.Count() + m_gBufferImages.Count(), depthImages, depthRefs);

	DynamicArray<VkAttachmentReference> depthInputRefs;
	CreateInputAttachmentReferences(m_targetImages.Count() + m_gBufferImages.Count(), depthImages, depthInputRefs);

	// ---------------------------------------------------------------------------------
	// Subpasses

	//VkAttachmentReference colorAttachmentRefs[] = { colorAttachRef, posAttachmentRef, normalAttachmentRef };

	VkSubpassDescription gBufferSubpass = {};
	gBufferSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	gBufferSubpass.colorAttachmentCount = gBufferRefs.Count();
	gBufferSubpass.pColorAttachments = gBufferRefs.Data();
	gBufferSubpass.pDepthStencilAttachment = depthRefs.Data();

	//VkAttachmentReference lightingInputs[] = { colorInputAttachRef, posInputAttachmentRef, normalInputAttachmentRef };

	VkSubpassDescription lightingSubpass = {};
	lightingSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	lightingSubpass.colorAttachmentCount = 1;
	lightingSubpass.pColorAttachments = &targetRefs[0];
	lightingSubpass.inputAttachmentCount = gBufferInputRefs.Count();
	lightingSubpass.pInputAttachments = gBufferInputRefs.Data();
	lightingSubpass.pDepthStencilAttachment = VK_NULL_HANDLE; // No depth needed.

	//VkAttachmentDescription attachments[] = { swapChainAttachment, depthAttachment, colorAttachment, posAttachment, normalAttachment };
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
