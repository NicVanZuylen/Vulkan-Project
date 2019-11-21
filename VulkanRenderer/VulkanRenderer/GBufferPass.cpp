#include "GBufferPass.h"
#include "RenderObject.h"
#include "SubScene.h"
#include "Material.h"

VkCommandBufferInheritanceInfo GBufferPass::m_inheritanceInfo =
{
	VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
	nullptr,
	VK_NULL_HANDLE,
	DYNAMIC_SUBPASS_INDEX,
	VK_NULL_HANDLE,
	VK_FALSE,
	0,
	0
};

VkCommandBufferBeginInfo GBufferPass::m_beginInfo = 
{
	VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
	nullptr,
	VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT | VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
	&GBufferPass::m_inheritanceInfo
}; 

GBufferPass::GBufferPass(Renderer* renderer, DynamicArray<PipelineData*>* pipelines, VkCommandPool cmdPool, VkRenderPass pass, VkDescriptorSet* mvpUBOSets, uint32_t nQueueFamilyIndex)
	: RenderModule(renderer, cmdPool, pass, nQueueFamilyIndex, false)
{
	m_renderer = renderer;
	m_pipelines = pipelines;
	m_nQueueFamilyIndex = nQueueFamilyIndex;
	std::memcpy(m_mvpUBODescSets, mvpUBOSets, sizeof(VkDescriptorSet) * MAX_FRAMES_IN_FLIGHT);

	m_inheritanceInfo.renderPass = m_renderPass;
}

GBufferPass::~GBufferPass()
{

}

void GBufferPass::RecordCommandBuffer(const uint32_t& nPresentImageIndex, const uint32_t& nFrameIndex, const VkFramebuffer& framebuffer, const VkCommandBuffer transferCmdBuf)
{
	// Set inheritence framebuffer & render pass.
	m_inheritanceInfo.framebuffer = framebuffer;
	m_inheritanceInfo.renderPass = m_renderPass;

	VkCommandBuffer cmdBuf = m_cmdBuffers[nFrameIndex];

	// Begin recording...
	RENDERER_SAFECALL(vkBeginCommandBuffer(cmdBuf, &m_beginInfo), "GBufferPass Error: Failed to begin recording of draw commands.");

	DynamicArray<PipelineData*>& pipelines = *m_pipelines;

	// Iterate through all pipelines for the subscene and draw their renderobjects.
	for (uint32_t i = 0; i < pipelines.Count(); ++i)
	{
		PipelineData& data = *pipelines[i];

		// Bind pipelines...
		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, data.m_handle);

		data.m_material->UseDescriptorSet(cmdBuf, data.m_layout, m_mvpUBODescSets[nFrameIndex], nFrameIndex);

		for (uint32_t j = 0; j < data.m_renderObjects.Count(); ++j)
		{
			RenderObject& obj = *data.m_renderObjects[j];

			// Request instance data update for next frame & draw current state of the renderobject.
			obj.UpdateInstanceData(transferCmdBuf);
			obj.CommandDraw(cmdBuf);
		}
	}

	// End recording...
	RENDERER_SAFECALL(vkEndCommandBuffer(cmdBuf), "GBufferPass Error: Failed to end recording of draw commands.");
}

void GBufferPass::OnOutputResize(const RenderModuleResizeData& resizeData)
{
	// Update render pass handle.
	m_renderPass = resizeData.m_renderPass;

	// Update MVP UBO descriptor sets.
	std::memcpy(m_mvpUBODescSets, resizeData.m_mvpUBOSets, sizeof(VkDescriptorSet) * MAX_FRAMES_IN_FLIGHT);
}
