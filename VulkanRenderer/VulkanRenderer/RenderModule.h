#pragma once
#include <vulkan/vulkan.h>
#include "DynamicArray.h"
#include "Renderer.h"

class RenderModule
{
public:

	RenderModule(Renderer* renderer, VkCommandPool cmdPool, VkRenderPass pass, unsigned int nQueueFamilyIndex, bool bStatic);

	virtual ~RenderModule() = 0;

	virtual void RecordCommandBuffer(const uint32_t& nPresentImageIndex, const uint32_t& nFrameIndex, const VkFramebuffer& framebuffer, const VkCommandBuffer transferCmdBuf) = 0;

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

	VkRenderPass m_renderPass;

	// ---------------------------------------------------------------------------------
	// Command pool

	VkCommandPool m_cmdPool;

	// ---------------------------------------------------------------------------------
	// Command buffers

	DynamicArray<VkCommandBuffer> m_cmdBuffers;
};

