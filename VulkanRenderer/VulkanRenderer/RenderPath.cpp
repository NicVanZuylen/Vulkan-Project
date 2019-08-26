#include "RenderPath.h"
#include "Renderer.h"

#include "Texture.h"

RenderPath::RenderPath(Renderer* renderer)
{
	m_renderer = renderer;

	CreatePass();

	CreateFramebuffer();

	CreateCommandPool();

	AllocateCommandBuffers();
}

RenderPath::~RenderPath()
{

}

void RenderPath::AddAttachment(Texture* texture, EAttachmentType type) 
{
	AttachmentInfo newAttachment;
	newAttachment.m_attachmentTex = texture;
	newAttachment.m_type = type;

	m_attachments.Push(newAttachment);
}

void RenderPath::AddSubpass(Subpass& subpass) 
{
	m_subpasses.Push(subpass);
}

void RenderPath::CreatePass() 
{
	VkRenderPassCreateInfo passCreateInfo = {};
	passCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	passCreateInfo.attachmentCount = m_attachments.Count();
	passCreateInfo.subpassCount = m_subpasses.Count();

	DynamicArray<VkAttachmentDescription> attachmentDescriptions;

	// Create attachment descriptions for each subpass attachment.
	for (int i = 0; i < m_attachments.Count(); ++i)
	{
		AttachmentInfo& attachment = m_attachments[i];

		AttachmentData data;

		VkAttachmentDescription& desc = data.m_description;
		desc.format = attachment.m_attachmentTex->Format();
		desc.samples = VK_SAMPLE_COUNT_1_BIT;

		VkAttachmentReference& readRef = data.m_readRef;
		readRef.attachment = i;

		VkAttachmentReference& writeRef = data.m_writeRef;
		writeRef.attachment = i;

		switch (attachment.m_attachmentTex->GetAttachmentType())
		{
		case ATTACHMENT_COLOR:

			desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;

			desc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			desc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

			desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

			writeRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			readRef.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			m_colorDataTable[attachment.m_attachmentTex] = data;
			attachmentDescriptions.Push(desc);

			break;

		case ATTACHMENT_DEPTH_STENCIL:

			desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;

			desc.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			desc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

			if (attachment.m_attachmentTex->HasStencil())
			{
				desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
				desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
			}
			else
			{
				// Dont care about stencil if the format doesnt support it.
				desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			}

			writeRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			readRef.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			m_depthDataTable[attachment.m_attachmentTex] = data;
			attachmentDescriptions.Push(desc);

			break;

		default:
			break;
		}
	}

	DynamicArray<DynamicArray<VkAttachmentReference>> subpassColorAttRefs;
	DynamicArray<VkAttachmentReference*> subpassDepthAttRefs;

	DynamicArray<DynamicArray<VkAttachmentReference>> subpassInputAttRefs;

	DynamicArray<VkSubpassDescription> subpassDescriptions;
	DynamicArray<VkSubpassDependency> subpassDependencies;

	for(int i = 0; i < m_subpasses.Count(); ++i) 
	{
		VkSubpassDescription subpassDesc = {};
		subpassDesc.colorAttachmentCount = 0;
		subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

		Subpass& subpass = m_subpasses[i];

		subpass.m_bHasColor = false;
		subpass.m_bHasDepthStencil = false;

		// Get correct attachment references via pointer hashing.
		for (int j = 0; j < subpass.m_attachmentInfos.Count(); ++j) 
		{
			if (subpass.m_attachmentInfos[j].m_type == ATTACHMENT_COLOR) 
			{
				if (subpass.m_attachmentInfos[j].m_usage == ATTACHMENT_USAGE_WRITE) 
				{
					subpassColorAttRefs[i].Push(m_colorDataTable[subpass.m_attachmentInfos[j].m_attachmentTex].m_writeRef);

				    ++subpassDesc.colorAttachmentCount;
					subpass.m_bHasColor = true;
				}
				else
					subpassInputAttRefs[i].Push(m_colorDataTable[subpass.m_attachmentInfos[j].m_attachmentTex].m_readRef);
			}
			else 
			{
				if (subpass.m_attachmentInfos[j].m_usage == ATTACHMENT_USAGE_WRITE) 
				{
				    subpassDepthAttRefs[i] = &m_depthDataTable[subpass.m_attachmentInfos[j].m_attachmentTex].m_writeRef;

					subpass.m_bHasDepthStencil = true;
				}
				else
					subpassInputAttRefs[i].Push(m_depthDataTable[subpass.m_attachmentInfos[j].m_attachmentTex].m_readRef);
			}
		}

		if(i == 0) 
		{
			// Initial subpass dependency defines the dependency between the first implicit subpass and the first explicit subpass.

			VkSubpassDependency initialDependency = {};
			initialDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
			initialDependency.dstSubpass = i;

			if (subpass.m_bHasColor)
				initialDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

			if (subpass.m_bHasDepthStencil)
				initialDependency.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

			initialDependency.srcAccessMask = 0;

			if(subpass.m_bHasColor)
			    initialDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

			if(subpass.m_bHasDepthStencil)
				initialDependency.dstAccessMask |= (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

			subpassDependencies.Push(initialDependency);
		}
		else if(subpass.m_nStageDependencyIndex != (~0U))
		{
			VkSubpassDependency dependency = {};
			dependency.srcSubpass = subpass.m_nStageDependencyIndex;
			dependency.dstSubpass = i;

			dependency.srcStageMask = subpass.m_dependentStages;
			
			if (subpass.m_bHasColor)
				dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

			if (subpass.m_bHasDepthStencil)
				dependency.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

			dependency.srcAccessMask = 0;

			if (subpass.m_bHasColor)
				dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

			if (subpass.m_bHasDepthStencil)
				dependency.dstAccessMask |= (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

			subpassDependencies.Push(dependency);
		}

		subpassDesc.pColorAttachments = subpassColorAttRefs[i].Data();
		subpassDesc.pDepthStencilAttachment = subpassDepthAttRefs[i];

		subpassDesc.pInputAttachments = subpassInputAttRefs[i].Data();
		subpassDesc.inputAttachmentCount = subpassInputAttRefs[i].Count();

		subpassDescriptions.Push(subpassDesc);
	}

	passCreateInfo.dependencyCount = subpassDependencies.Count();

	passCreateInfo.pAttachments = attachmentDescriptions.Data();
	passCreateInfo.pSubpasses = subpassDescriptions.Data();
	passCreateInfo.pDependencies = subpassDependencies.Data();

	RENDERER_SAFECALL(vkCreateRenderPass(m_renderer->GetDevice(), &passCreateInfo, nullptr, &m_pass), "Critical Renderer Error: Failed to create render path pass.");
}

void RenderPath::CreateFramebuffer() 
{
	
}

void RenderPath::CreateCommandPool() 
{

}

void RenderPath::AllocateCommandBuffers() 
{

}
