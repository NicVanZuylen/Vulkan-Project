#include "MeshRenderer.h"
#include "VertexInfo.h"
#include "Mesh.h"
#include "Shader.h"
#include "Material.h"
#include "Renderer.h"

PipelineData::PipelineData() 
{
	m_handle = nullptr;
	m_layout = nullptr;
	m_material = nullptr;
}

PipelineDataPtr::PipelineDataPtr() 
{
	m_ptr = nullptr;
}

VkVertexInputBindingDescription VertexType::BindingDescription() 
{
	VkVertexInputBindingDescription bindDesc = {};
	bindDesc.binding = 0;
	bindDesc.stride = sizeof(glm::vec4);
	bindDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; // Data is per-vertex rather than per-instance.

	return bindDesc;
}

void VertexType::AttributeDescriptions(DynamicArray<VkVertexInputAttributeDescription>& outDescriptions)
{
	VkVertexInputAttributeDescription defaultDesc = {};
	defaultDesc.binding = 0;
	defaultDesc.location = 0;
	defaultDesc.offset = 0;
	defaultDesc.format = VK_FORMAT_R32G32B32A32_SFLOAT; // vec4 format.

	outDescriptions = { defaultDesc };
}

Table<PipelineDataPtr> MeshRenderer::m_pipelineTable;
DynamicArray<PipelineData*> MeshRenderer::m_allPipelines;

MeshRenderer::MeshRenderer(Renderer* renderer, Mesh* mesh, Material* material)
{
	m_renderer = renderer;
	m_mesh = mesh;
	m_material = material;
	m_pipelineData = nullptr;

	m_nameID = "|" + material->GetName() + mesh->VertexFormat()->NameID();

	CreateGraphicsPipeline();
}

MeshRenderer::~MeshRenderer()
{
	
}

DynamicArray<PipelineData*>& MeshRenderer::Pipelines() 
{
	return m_allPipelines;
}

void MeshRenderer::CommandDraw(VkCommandBuffer_T* cmdBuffer) 
{
	Mesh& meshRef = *m_mesh;

	meshRef.Bind(cmdBuffer);

	// Draw...
	vkCmdDrawIndexed(cmdBuffer, meshRef.IndexCount(), 1, 0, 0, 0);
}

const Shader* MeshRenderer::GetShader() const
{
	return m_material->GetShader();
}

const Material* MeshRenderer::GetMaterial() const 
{
	return m_material;
}

void MeshRenderer::CreateGraphicsPipeline() 
{
	PipelineData*& pipelineData = m_pipelineTable[{ m_nameID.c_str() }].m_ptr;

	if (pipelineData)
	{
		// Pipeline already exists. This class does not own it but can use it.
		m_pipelineData = pipelineData;
		m_pipelineData->m_renderObjects.Push(this);

		return;
	}

	// Vertex shader stage information.
	VkPipelineShaderStageCreateInfo vertStageInfo = {};
	vertStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertStageInfo.module = m_material->GetShader()->m_vertModule;
	vertStageInfo.pName = "main";

	// Fragment shader stage information.
	VkPipelineShaderStageCreateInfo fragStageInfo = {};
	fragStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragStageInfo.module = m_material->GetShader()->m_fragModule;
	fragStageInfo.pName = "main";

	// Array of shader stage information.
	VkPipelineShaderStageCreateInfo shaderStageInfos[] = { vertStageInfo, fragStageInfo };

	const VertexInfo& vertFormat = *m_mesh->VertexFormat();

	// Vertex attribute information.
	auto bindingDesc = vertFormat.BindingDescription();
	const DynamicArray<VkVertexInputAttributeDescription>& attrDescriptions = vertFormat.AttributeDescriptions();

	VkPipelineVertexInputStateCreateInfo vertInputInfo = {};
	vertInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertInputInfo.vertexBindingDescriptionCount = 1;
	vertInputInfo.pVertexBindingDescriptions = &bindingDesc;
	vertInputInfo.vertexAttributeDescriptionCount = attrDescriptions.Count();
	vertInputInfo.pVertexAttributeDescriptions = attrDescriptions.Data();

	// Input assembly stage configuration.
	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	// Viewport configuration.
	VkViewport viewPort = {};
	viewPort.x = 0.0f;
	viewPort.y = 0.0f;
	viewPort.width = (float)m_renderer->FrameWidth();
	viewPort.height = (float)m_renderer->FrameHeight();
	viewPort.minDepth = 0.0f;
	viewPort.maxDepth = 1.0f;

	// Scissor configuration.
	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = { m_renderer->FrameWidth(), m_renderer->FrameHeight() };

	// Viewport state configuration.
	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewPort;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	// Primitive rasterization stage configuration.
	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

	// Used for shadow mapping...
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f;
	rasterizer.depthBiasClamp = 0.0f;
	rasterizer.depthBiasSlopeFactor = 0.0f;

	// Multisampling stage configuration.
	VkPipelineMultisampleStateCreateInfo multisampler = {};
	multisampler.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampler.sampleShadingEnable = VK_FALSE;
	multisampler.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampler.minSampleShading = 1.0f;
	multisampler.pSampleMask = nullptr;
	multisampler.alphaToCoverageEnable = VK_FALSE;
	multisampler.alphaToOneEnable = VK_FALSE;

	// Depth/Stencil state
	// ---

	// Color blending
	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlending = {};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	// Dynamic states
	/*
	VkDynamicState dynState = VK_DYNAMIC_STATE_VIEWPORT;

	VkPipelineDynamicStateCreateInfo dynamicState = {};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = 1;
	dynamicState.pDynamicStates = &dynState;
	*/

	m_pipelineData = new PipelineData;
	m_pipelineData->m_renderObjects.Push(this);

	VkDescriptorSetLayout uboSetLayout = m_renderer->MVPUBOSetLayout();

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &uboSetLayout; // MVP UBO
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	RENDERER_SAFECALL(vkCreatePipelineLayout(m_renderer->GetDevice(), &pipelineLayoutInfo, nullptr, &m_pipelineData->m_layout), "Renderer Error: Failed to create graphics pipeline layout.");

	// Create pipeline.
	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStageInfos;
	pipelineInfo.pVertexInputState = &vertInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampler;
	pipelineInfo.pDepthStencilState = nullptr;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = nullptr;
	pipelineInfo.layout = m_pipelineData->m_layout;
	pipelineInfo.renderPass = m_renderer->DynamicRenderPass();
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	RENDERER_SAFECALL(vkCreateGraphicsPipelines(m_renderer->GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipelineData->m_handle), "Renderer Error: Failed to create graphics pipeline.");

	// Set pipeline material.
	m_pipelineData->m_material = m_material;

	// Set table pointer.
	pipelineData = m_pipelineData;

	// Add pipeline data to array of all pipelines.
	m_allPipelines.Push(m_pipelineData);
}