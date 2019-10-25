#pragma once
#include <vulkan/vulkan.h>
#include "DynamicArray.h"

class Renderer;

class RenderModule
{
public:

	RenderModule(Renderer* renderer, unsigned int nQueueFamilyIndex, bool bStatic);

	virtual ~RenderModule() = 0;

	virtual void RecordCommandBuffer(unsigned int nBufferIndex, unsigned int nFrameIndex) = 0;

	VkCommandBuffer GetCommandBuffer(unsigned int nBufferIndex);

private:

	inline void CreateCommandPool();

	inline void CreateCommandBuffers();

protected:

	// ---------------------------------------------------------------------------------
	// Main

	Renderer* m_renderer;
	unsigned int m_nQueueFamilyIndex;
	bool m_bStatic;

	// ---------------------------------------------------------------------------------
	// Command pool

	VkCommandPool m_cmdPool;

	// ---------------------------------------------------------------------------------
	// Command buffers

	DynamicArray<VkCommandBuffer> m_cmdBuffers;
};

