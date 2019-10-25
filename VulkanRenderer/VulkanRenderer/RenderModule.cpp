#include "RenderModule.h"
#include "Renderer.h"

RenderModule::RenderModule(Renderer* renderer, unsigned int nQueueFamilyIndex, bool bStatic)
{
	m_renderer = renderer;
	m_nQueueFamilyIndex = nQueueFamilyIndex;
	m_bStatic = bStatic;

	CreateCommandPool();
	CreateCommandBuffers();
}

RenderModule::~RenderModule() 
{
	vkDestroyCommandPool(m_renderer->GetDevice(), m_cmdPool, nullptr);
}

VkCommandBuffer RenderModule::GetCommandBuffer(unsigned int nBufferIndex)
{
	return m_cmdBuffers[nBufferIndex];
}

inline void RenderModule::CreateCommandPool()
{
	// Command pool create info.
	VkCommandPoolCreateInfo poolCreateInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		nullptr,
		VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // Command buffers will often be re-recorded.
		m_nQueueFamilyIndex
	};

	// Command buffers will not be re-recorded if static after the initial recording.
	if (m_bStatic)
		poolCreateInfo.flags = 0;

	// Create command pool.
	RENDERER_SAFECALL(vkCreateCommandPool(m_renderer->GetDevice(), &poolCreateInfo, nullptr, &m_cmdPool), "Module Error: Failed to allocate module command pool.");
}

inline void RenderModule::CreateCommandBuffers()
{
	// Allocate handle memory for command buffers.
	m_cmdBuffers.SetSize(m_renderer->SwapChainImageCount());
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
