#pragma once
#include <vulkan/vulkan.h>

#include "DynamicArray.h"
#include "PairTable.h"

class Renderer;
class Texture;

#ifndef ATTACHMENT_E
#define ATTACHMENT_E

enum EAttachmentType
{
	ATTACHMENT_COLOR,
	ATTACHMENT_DEPTH_STENCIL
};

#endif

enum EAttachmentUsageFlags 
{
	ATTACHMENT_USAGE_READ,
	ATTACHMENT_USAGE_WRITE
};

struct AttachmentInfo 
{
	Texture* m_attachmentTex;
	EAttachmentType m_type;
	EAttachmentUsageFlags m_usage;
};

struct AttachmentData 
{
	VkAttachmentDescription m_description;
	VkAttachmentReference m_readRef;
	VkAttachmentReference m_writeRef;
};

struct Subpass 
{
	DynamicArray<AttachmentInfo> m_attachmentInfos;
	uint32_t m_nStageDependencyIndex;
	VkPipelineStageFlagBits m_dependentStages;

	bool m_bHasColor;
	bool m_bHasDepthStencil;
	bool m_bUsesColorInput;
	bool m_bUsesDepthStencilInput;
	bool m_bIndependent;
};

class RenderPath
{
public:

	RenderPath(Renderer* renderer);

	~RenderPath();

	void AddAttachment(Texture* texture, EAttachmentType type);

	void AddSubpass(Subpass& subpass);

	void CreatePass();

private:

	void CreateFramebuffer();

	void CreateCommandPool();

	void AllocateCommandBuffers();

	Renderer* m_renderer;

	// Render pass & subpasses
	VkRenderPass m_pass;
	DynamicArray<AttachmentInfo> m_attachments;
	DynamicArray<Subpass> m_subpasses;
	PairTable<AttachmentData, Texture*> m_colorDataTable;
	PairTable<AttachmentData, Texture*> m_depthDataTable;

	// Command buffers.
	VkCommandPool m_cmdPool;

	VkCommandBuffer m_primaryCmdBuffer;
	DynamicArray<VkCommandBuffer> m_subpassSecondaryCmdBufs;
};

