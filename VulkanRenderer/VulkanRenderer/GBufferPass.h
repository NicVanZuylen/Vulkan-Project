#pragma once
#include "RenderModule.h"

class RenderObject;

struct PipelineData;

class GBufferPass : public RenderModule
{
public:

	GBufferPass(Renderer* renderer, DynamicArray<PipelineData*>* pipelines, VkCommandPool cmdPool, VkRenderPass pass, VkDescriptorSet* mvpUBOSets, uint32_t nQueueFamilyIndex);

	~GBufferPass();

	void RecordCommandBuffer(const uint32_t& nPresentImageIndex, const uint32_t& nFrameIndex, const VkFramebuffer& framebuffer, const VkCommandBuffer transferCmdBuf) override;

private:

	// ---------------------------------------------------------------------------------
	// Template Vulkan structures

	static VkCommandBufferInheritanceInfo m_inheritanceInfo;
	static VkCommandBufferBeginInfo m_beginInfo;

	// ---------------------------------------------------------------------------------
	// Descriptors

	VkDescriptorSet m_mvpUBODescSets[MAX_FRAMES_IN_FLIGHT];

	// ---------------------------------------------------------------------------------
	// Scene data

	DynamicArray<PipelineData*>* m_pipelines;

	// ---------------------------------------------------------------------------------
};

