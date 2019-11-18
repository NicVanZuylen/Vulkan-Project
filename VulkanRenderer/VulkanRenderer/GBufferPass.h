#pragma once
#include "RenderModule.h"

class RenderObject;

struct PipelineData;

class GBufferPass : public RenderModule
{
public:

	GBufferPass(Renderer* renderer, DynamicArray<PipelineData*>* pipelines, VkCommandPool cmdPool, VkFramebuffer* framebuffers, VkRenderPass pass, uint32_t nQueueFamilyIndex);

	~GBufferPass();

	void RecordCommandBuffer(uint32_t nPresentImageIndex, uint32_t nFrameIndex) override;

private:

	// ---------------------------------------------------------------------------------
	// Template Vulkan structures

	static VkCommandBufferInheritanceInfo m_inheritanceInfo;
	static VkCommandBufferBeginInfo m_beginInfo;

	// ---------------------------------------------------------------------------------
	// Scene data

	DynamicArray<PipelineData*>* m_pipelines;

	// ---------------------------------------------------------------------------------
};

