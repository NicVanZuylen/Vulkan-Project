#pragma once
#include <vulkan/vulkan.h>
#include "DynamicArray.h"
#include "Renderer.h"

class RenderModule
{
public:

	RenderModule(Renderer* renderer, VkCommandPool cmdPool, VkFramebuffer* frameBuffers, VkRenderPass pass, unsigned int nQueueFamilyIndex, bool bStatic);

	virtual ~RenderModule() = 0;

	virtual void RecordCommandBuffer(unsigned int nBufferIndex, unsigned int nFrameIndex) = 0;

	const VkCommandBuffer* GetCommandBuffer(unsigned int nBufferIndex);

private:

	inline void CreateCommandBuffers();

protected:

	// ---------------------------------------------------------------------------------
	// Main

	Renderer* m_renderer;
	unsigned int m_nQueueFamilyIndex;
	bool m_bStatic;

	// ---------------------------------------------------------------------------------
	// Rendering

	VkFramebuffer m_frameBuffers[MAX_FRAMES_IN_FLIGHT];
	VkRenderPass m_renderPass;

	// ---------------------------------------------------------------------------------
	// Command pool

	VkCommandPool m_cmdPool;

	// ---------------------------------------------------------------------------------
	// Command buffers

	DynamicArray<VkCommandBuffer> m_cmdBuffers;
};

