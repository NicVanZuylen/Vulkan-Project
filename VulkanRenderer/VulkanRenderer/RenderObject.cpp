#include "RenderObject.h"
#include "VertexInfo.h"
#include "Mesh.h"
#include "Shader.h"
#include "Material.h"
#include "Renderer.h"
#include "SubScene.h"

DynamicArray<EVertexAttribute> RenderObject::m_defaultInstanceAttributes = { VERTEX_ATTRIB_FLOAT4, VERTEX_ATTRIB_FLOAT4, VERTEX_ATTRIB_FLOAT4, VERTEX_ATTRIB_FLOAT4 };

RenderObject::RenderObject(Scene* scene, Mesh* mesh, Material* material, DynamicArray<EVertexAttribute>* instanceAttributes, uint32_t nMaxInstanceCount, uint32_t nSubScenebits)
{
	m_scene = scene;
	m_subScene = scene->GetPrimarySubScene();
	m_renderer = scene->GetRenderer();
	m_mesh = mesh;
	m_material = material;
	m_pipelineData = nullptr;
	
	m_instanceArray = new Instance[nMaxInstanceCount];
	m_nInstanceArraySize = nMaxInstanceCount;
	m_nInstanceCount = 0;
	m_bInstancesModified = true;

	m_nameID = "|" + material->GetName() + mesh->VertexFormat()->NameID();

	m_nSubSceneBits = nSubScenebits;

	CreateGraphicsPipeline(instanceAttributes);

	// Add first instance.
	Instance firstInstance = { glm::mat4() };
	AddInstance(firstInstance);
}

RenderObject::~RenderObject()
{
	if(m_instanceArray) 
	{
	    delete[] m_instanceArray;

		m_renderer->WaitGraphicsIdle();
		m_renderer->WaitTransferIdle();

		// Destroy instance staging buffer & memory.
		vkDestroyBuffer(m_renderer->GetDevice(), m_instanceStagingBuffer, nullptr);
		vkFreeMemory(m_renderer->GetDevice(), m_instanceStagingMemory, nullptr);

		m_instanceStagingBuffer = nullptr;
		m_instanceStagingMemory = nullptr;

		// Destroy instance buffer & memory.
		vkDestroyBuffer(m_renderer->GetDevice(), m_instanceBuffer, nullptr);
		vkFreeMemory(m_renderer->GetDevice(), m_instanceMemory, nullptr);

		m_instanceBuffer = nullptr;
		m_instanceMemory = nullptr;
	}

	// Remove this render object from the pipeline.
	m_pipelineData->m_renderObjects.Pop(this);

	if (m_pipelineData->m_renderObjects.Count() == 0) // This is the last object using the pipeline, destroy the pipeline.
	{
		// Wait for graphics & transfer queues to be idle.
		m_renderer->WaitGraphicsIdle();
		m_renderer->WaitTransferIdle();

		// Destroy pipeline objects.
		vkDestroyPipeline(m_renderer->GetDevice(), m_pipelineData->m_handle, nullptr);
		vkDestroyPipelineLayout(m_renderer->GetDevice(), m_pipelineData->m_layout, nullptr);

		delete m_pipelineData;
		m_pipelineData = nullptr;
	}
}

void RenderObject::CommandDraw(VkCommandBuffer_T* cmdBuffer) 
{
	Mesh& meshRef = *m_mesh;

	// Bind vertex, index and instance buffers.
	meshRef.Bind(cmdBuffer, m_instanceBuffer);

	// Draw...
	vkCmdDrawIndexed(cmdBuffer, meshRef.IndexCount(), m_nInstanceCount, 0, 0, 0);
}

void RenderObject::AddInstance(Instance& instance) 
{
	// Don't attempt to add beyond the max instance limit.
	if (m_nInstanceCount >= m_nInstanceArraySize)
		return;

	m_instanceArray[m_nInstanceCount++] = instance;
	m_bInstancesModified = true;
}

void RenderObject::RemoveInstance(const unsigned int& nIndex)
{
	if (m_nInstanceCount <= 0)
		return;

	// Copy the contents beyond the provided index to the provided index to overlap the old data, and close the gap.
	if (nIndex < m_nInstanceCount - 1)
	{
		unsigned int nCopySize = (m_nInstanceCount - (nIndex + 1)) * sizeof(Instance);
		memcpy_s(&m_instanceArray[nIndex], nCopySize, &m_instanceArray[nIndex + 1], nCopySize);
	}
	
	--m_nInstanceCount;
}

void RenderObject::SetInstance(const unsigned int& nIndex, Instance& instance) 
{
	// Don't attempt to modify beyond the max instance limit.
	if (nIndex < m_nInstanceCount) 
	{
	    m_instanceArray[nIndex] = instance;

		m_bInstancesModified = true;
	}
}

void RenderObject::UpdateInstanceData(VkCommandBuffer cmdBuffer)
{
	if (!m_bInstancesModified || m_nInstanceCount == 0)
		return;

	int nCopySize = sizeof(Instance) * m_nInstanceCount;

	// Map instance staging buffer.
	void* bufferPtr = nullptr;
	RENDERER_SAFECALL(vkMapMemory(m_renderer->GetDevice(), m_instanceStagingMemory, 0, nCopySize, 0, &bufferPtr), "RenderObject error: Failed to update instance data on GPU.");

	// Copy data...
	memcpy_s(bufferPtr, nCopySize, m_instanceArray, nCopySize);

	// Unmap buffer.
	vkUnmapMemory(m_renderer->GetDevice(), m_instanceStagingMemory);

	// Copy instance staging buffer to device local instance buffer.
	VkBufferCopy insCopyRegion = {};
	insCopyRegion.srcOffset = 0;
	insCopyRegion.dstOffset = 0;
	insCopyRegion.size = sizeof(Instance) * m_nInstanceCount;

	vkCmdCopyBuffer(cmdBuffer, m_instanceStagingBuffer, m_instanceBuffer, 1, &insCopyRegion);

	// Create and submit copy request.
	//CopyRequest bufferCopyRequest = { m_instanceStagingBuffer, m_instanceBuffer, insCopyRegion };
	//m_renderer->RequestCopy(bufferCopyRequest);

	/*
	Renderer::TempCmdBuffer tempCmdBuf = m_renderer->CreateTempCommandBuffer();

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0;
	beginInfo.pInheritanceInfo = nullptr;
	beginInfo.pNext = nullptr;

	vkBeginCommandBuffer(tempCmdBuf.m_handle, &beginInfo);

	VkBufferCopy copyRegion;
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = sizeof(Instance) * m_nInstanceCount;

	vkCmdCopyBuffer(tempCmdBuf.m_handle, m_instanceStagingBuffer, m_instanceBuffer, 1, &copyRegion);

	vkEndCommandBuffer(tempCmdBuf.m_handle);

	m_renderer->UseAndDestroyTempCommandBuffer(tempCmdBuf);
	*/
	

	m_bInstancesModified = false;
}

void RenderObject::RecreatePipeline() 
{
	CreateGraphicsPipeline(&m_pipelineData->m_vertexAttributes, true);
}

const Shader* RenderObject::GetShader() const
{
	return m_material->GetShader();
}

const Material* RenderObject::GetMaterial() const 
{
	return m_material;
}

PipelineData* RenderObject::GetPipeline()
{
	return m_pipelineData;
}

void RenderObject::CreateGraphicsPipeline(DynamicArray<EVertexAttribute>* vertexAttributes, bool bRecreate)
{
	// -------------------------------------------------------------------------------------------------------------------
	// Instance buffer

	VertexInfo insVertInfo(*vertexAttributes, true, m_mesh->VertexFormat());

	// Create instance staging buffer.
	if(!m_instanceStagingBuffer && !m_instanceStagingMemory)
	    m_renderer->CreateBuffer(m_nInstanceArraySize * sizeof(Instance), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_instanceStagingBuffer, m_instanceStagingMemory);

	// Create device local instance buffer.
	if(!m_instanceBuffer && !m_instanceMemory)
	    m_renderer->CreateBuffer(m_nInstanceArraySize * sizeof(Instance), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_instanceBuffer, m_instanceMemory);

	// -------------------------------------------------------------------------------------------------------------------
	// Check for existing matching pipeline in any subscene.

	const uint32_t nSubSceneCount = 1;

	Table<PipelineDataPtr>& pipelines = m_subScene->GetPipelineTable();

	PipelineData*& pipelineData = pipelines[{ m_nameID.c_str() }].m_ptr;

	if (pipelineData && !bRecreate)
	{
		// Pipeline already exists. This class does not own it but can use it.
		m_pipelineData = pipelineData;
		m_pipelineData->m_renderObjects.Push(this);

		return;
	}
	else if(bRecreate) 
	{
		// Destroy old pipelines, if they exist.

		// Destroy old pipeline layouts.
		if(pipelineData->m_layout) 
		{
		    vkDestroyPipelineLayout(m_renderer->GetDevice(), pipelineData->m_layout, nullptr);
			pipelineData->m_layout = nullptr;
		}

		// Destory old pipelines.
		if(pipelineData->m_handle) 
		{
		    vkDestroyPipeline(m_renderer->GetDevice(), pipelineData->m_handle, nullptr);
			pipelineData->m_handle = nullptr;
		}
	}
	
	// -------------------------------------------------------------------------------------------------------------------

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
	VkVertexInputBindingDescription bindingDescriptions[2] = { vertFormat.BindingDescription(), insVertInfo.BindingDescription() };

	int nDescCount = vertFormat.AttributeDescriptionCount() + insVertInfo.AttributeDescriptionCount();
	VkVertexInputAttributeDescription* attrDescriptions = new VkVertexInputAttributeDescription[nDescCount];

	// Copy vertex descriptions...
	memcpy_s(attrDescriptions, sizeof(VkVertexInputAttributeDescription) * nDescCount, vertFormat.AttributeDescriptions(), sizeof(VkVertexInputAttributeDescription) * vertFormat.AttributeDescriptionCount());

	// Copy instance descriptions...
	memcpy_s(&attrDescriptions[vertFormat.AttributeDescriptionCount()], sizeof(VkVertexInputAttributeDescription) * insVertInfo.AttributeDescriptionCount(), insVertInfo.AttributeDescriptions()
		, sizeof(VkVertexInputAttributeDescription) * insVertInfo.AttributeDescriptionCount());

	VkPipelineVertexInputStateCreateInfo vertInputInfo = {};
	vertInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertInputInfo.vertexBindingDescriptionCount = 2;
	vertInputInfo.pVertexBindingDescriptions = bindingDescriptions;
	vertInputInfo.vertexAttributeDescriptionCount = nDescCount;
	vertInputInfo.pVertexAttributeDescriptions = attrDescriptions;

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
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL; // Fragment shader will be run on all rasterized fragments within each triangle.
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT; // Face culling
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

	// Used for shadow mapping...
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f;
	rasterizer.depthBiasClamp = 0.0f;
	rasterizer.depthBiasSlopeFactor = 0.0f;

	// Depth / Stencil state
	VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
	depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencilState.depthTestEnable = VK_TRUE;
	depthStencilState.depthWriteEnable = VK_TRUE;
	depthStencilState.stencilTestEnable = VK_FALSE;
	depthStencilState.depthBoundsTestEnable = VK_FALSE; // We don't need the bounds test.
	depthStencilState.minDepthBounds = 0.0f;
	depthStencilState.maxDepthBounds = 1.0f;
	depthStencilState.flags = 0;

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

	VkPipelineColorBlendAttachmentState colorBlendAttachments[] = { colorBlendAttachment, colorBlendAttachment, colorBlendAttachment };

	VkPipelineColorBlendStateCreateInfo colorBlending = {};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 3; // Blending for Color, Positions, and Normals attachments.
	colorBlending.pAttachments = colorBlendAttachments;
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	// If being recreated, the pipeline data structure is not reallocated and objects inside it remain.
	if (!pipelineData)
	{
		pipelineData = new PipelineData;
		pipelineData->m_renderObjects.Push(this);

		// Set pipeline material.
		pipelineData->m_material = m_material;

		// Copy vertex attributes.
		pipelineData->m_vertexAttributes = *vertexAttributes;

		// Set local pointer.
		m_pipelineData = pipelineData;

		// Add pipeline data to the subscene it will be rendered in.
		m_subScene->AddPipeline(pipelineData);
	}

	// Get descriptor layouts for MVP UBO & Material properties
	VkDescriptorSetLayout setLayouts[] = { m_subScene->MVPUBOLayout(), m_material->GetDescriptorLayout() };

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 2;
	pipelineLayoutInfo.pSetLayouts = setLayouts;
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
	pipelineInfo.pDepthStencilState = &depthStencilState;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = nullptr;
	pipelineInfo.layout = m_pipelineData->m_layout;
	pipelineInfo.renderPass = m_subScene->GetRenderPass();
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	RENDERER_SAFECALL(vkCreateGraphicsPipelines(m_renderer->GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipelineData->m_handle), "Renderer Error: Failed to create graphics pipeline.");

	delete[] attrDescriptions;
}