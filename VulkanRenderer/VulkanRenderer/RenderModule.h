#pragma once
#include <vulkan/vulkan.h>
#include "DynamicArray.h"
#include "Renderer.h"

struct RenderModuleResizeData 
{
	uint32_t m_nWidth; // New render output width.
	uint32_t m_nHeight; // New render output height.
	VkRenderPass m_renderPass; // New render pass handle.
	VkDescriptorSet* m_mvpUBOSets; // New MVP UBO handles MAX_FRAMES_IN_FLIGHT in count.
	VkDescriptorSet* m_gBufferSets; // New G-Buffer input attachment descriptor set.
};

class RenderModule
{
public:

	RenderModule(Renderer* renderer, VkCommandPool cmdPool, VkRenderPass pass, unsigned int nQueueFamilyIndex, bool bStatic);

	virtual ~RenderModule() = 0;

	virtual void RecordCommandBuffer(const uint32_t& nPresentImageIndex, const uint32_t& nFrameIndex, const VkFramebuffer& framebuffer, const VkCommandBuffer transferCmdBuf) = 0;

	/*
	Description: Called when the subscene output is resized.
	Param:
	    const RenderModuleResizeData& resizeData: Data containing the new width and height of the render output & updated handles.
	*/
	virtual void OnOutputResize(const RenderModuleResizeData& resizeData);

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

