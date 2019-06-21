#include "MeshRenderer.h"
#include "Shader.h"
#include "Renderer.h"

MeshRenderer::MeshRenderer(const Shader* shader, Renderer* renderer)
{
	m_shader = shader;
	m_renderer = renderer;

	m_commandBuffers = nullptr;


	CreateCommandBuffers();
	RecordCommandBuffers();
}

MeshRenderer::~MeshRenderer()
{
	if (m_commandBuffers)
		delete[] m_commandBuffers;
}

void MeshRenderer::CommandDraw(VkCommandBuffer_T* cmdBuffer) 
{
	// Bind pipeline...
	vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shader->m_pipeline->m_handle);

	// Draw...
	vkCmdDraw(cmdBuffer, 3, 1, 0, 0);
}

const Shader* MeshRenderer::GetShader() 
{
	return m_shader;
}

VkCommandBuffer MeshRenderer::GetDrawCommands(const unsigned int& frameBufferIndex) 
{
	return m_commandBuffers[frameBufferIndex];
}

void MeshRenderer::CreateCommandBuffers() 
{
	m_commandBufferCount = m_renderer->GetFramebuffers().Count();

	VkCommandBufferAllocateInfo allocateInfo = {};
	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.commandPool = m_renderer->GetCommandPool();
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.pNext = nullptr;
	allocateInfo.commandBufferCount = m_commandBufferCount;

	// Allocate command buffer array.
	if (!m_commandBuffers)
	    m_commandBuffers = new VkCommandBuffer[m_commandBufferCount];

	RENDERER_SAFECALL(vkAllocateCommandBuffers(m_renderer->GetDevice(), &allocateInfo, m_commandBuffers), "MeshRenderer Error: Failed to allocate command buffers.");
}

void MeshRenderer::RecordCommandBuffers() 
{
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	beginInfo.pInheritanceInfo = nullptr;
	beginInfo.pNext = nullptr;

	Renderer& renderRef = *m_renderer;

	VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };

	for(int i = 0; i < m_commandBufferCount; ++i) 
	{
		VkCommandBuffer& cmdBuffer = m_commandBuffers[i];

		RENDERER_SAFECALL(vkBeginCommandBuffer(cmdBuffer, &beginInfo), "MeshRenderer Error: Failed to begin command buffer recording.");

		// Render pass beginning.
		VkRenderPassBeginInfo passBeginInfo = {};
		passBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		passBeginInfo.renderPass = renderRef.MainRenderPass();
		passBeginInfo.framebuffer = renderRef.GetFramebuffers()[i];
		passBeginInfo.renderArea.offset = { 0, 0 };
		passBeginInfo.renderArea.extent = { m_renderer->FrameWidth(), m_renderer->FrameHeight() };

		passBeginInfo.clearValueCount = 0;
		//passBeginInfo.pClearValues = &clearColor;

		vkCmdBeginRenderPass(cmdBuffer, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		// Bind vertex & index buffers.

		// For now the pipeline is bound for each draw call using it. TODO: Only bind the pipeline once and subsequently draw all objects using it before binding the next pipeline.
		// Pipeline & Descriptor sets.
		vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shader->m_pipeline->m_handle);

		// Draw...
		vkCmdDraw(cmdBuffer, 3, 1, 0, 0);

		vkCmdEndRenderPass(cmdBuffer);

		RENDERER_SAFECALL(vkEndCommandBuffer(cmdBuffer), "MeshRenderer Error: Failed to end command buffer recording.");
	}
}
