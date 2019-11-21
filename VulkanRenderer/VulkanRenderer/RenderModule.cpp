#include "RenderModule.h"
#include "Renderer.h"

RenderModule::RenderModule(Renderer* renderer, VkCommandPool cmdPool, VkRenderPass pass, unsigned int nQueueFamilyIndex, bool bStatic)
{
	m_renderer = renderer;
	m_nQueueFamilyIndex = nQueueFamilyIndex;
	m_bStatic = bStatic;

	m_renderPass = pass;

	m_cmdPool = cmdPool;

	CreateCommandBuffers();
}

RenderModule::~RenderModule() 
{

}

void RenderModule::OnOutputResize(const RenderModuleResizeData& resizeData)
{
	m_renderPass = resizeData.m_renderPass;
}

const VkCommandBuffer* RenderModule::GetCommandBuffer(unsigned int nBufferIndex)
{
	return &m_cmdBuffers[nBufferIndex];
}

inline void RenderModule::CreateCommandBuffers()
{
	// Allocate handle memory for command buffers. One for each frame-in-flight.
	m_cmdBuffers.SetSize(MAX_FRAMES_IN_FLIGHT);
	m_cmdBuffers.SetCount(m_cmdBuffers.GetSize());

	// Allocation info...
	VkCommandBufferAllocateInfo allocInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		nullptr,
		m_cmdPool,
		VK_COMMAND_BUFFER_LEVEL_SECONDARY,
		static_cast<uint32_t>(m_cmdBuffers.Count()),
	};

	// Allocate command buffers.
	RENDERER_SAFECALL(vkAllocateCommandBuffers(m_renderer->GetDevice(), &allocInfo, m_cmdBuffers.Data()), "Module Error: Failed to allocate module command buffers.");
}
