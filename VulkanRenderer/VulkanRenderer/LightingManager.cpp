#include "LightingManager.h"
#include "Renderer.h"
#include "VertexInfo.h"
#include "Mesh.h"
#include "Shader.h"

LightingManager::LightingManager(Renderer* renderer, Shader* dirLightShader, Shader* pointLightShader, const unsigned int& nWindowWidth, const unsigned int& nWindowHeight)
{
	m_renderer = renderer;

	m_dirLights.SetSize(DIRECTIONAL_LIGHT_COUNT);
	m_dirLights.SetCount(DIRECTIONAL_LIGHT_COUNT);

	for(int i = 0; i < DIRECTIONAL_LIGHT_COUNT; ++i) 
	{
		m_dirLights[i].m_v4Direction = glm::vec4(0.0f, -1.0f, 0.0f, 1.0f); // Point directly down.
		m_dirLights[i].m_v4Color = glm::vec4(1.0f); // White
	}

	// One UBO for all direction lights, to avoid memory fragmentation.
	m_renderer->CreateBuffer(sizeof(DirectionalLight) * DIRECTIONAL_LIGHT_COUNT, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_dirLightUBO, m_dirLightUBOMemory);

	m_pointLightVolMesh = new Mesh(m_renderer, "Assets/Primitives/sphere.obj", &Mesh::defaultFormat);

	m_nChangePLightStart = 0U;
	m_nChangePLightEnd = 1U;
	m_bPointLightChange = false;

	CreatePointLightBuffers();
	CreateDescriptorPool();
	CreateSetLayouts();
	CreateDescriptorSets();
	CreateDirLightingPipeline(dirLightShader, nWindowWidth, nWindowHeight);
	CreatePointLightingPipeline(pointLightShader, nWindowWidth, nWindowHeight);

	UpdateDirLights();

	m_bDirLightChange = false;
}

LightingManager::~LightingManager()
{
	// Destroy pipelines.
	vkDestroyPipeline(m_renderer->GetDevice(), m_dirLightPipeline, nullptr);
	vkDestroyPipelineLayout(m_renderer->GetDevice(), m_dirLightPipelineLayout, nullptr);

	vkDestroyPipeline(m_renderer->GetDevice(), m_pointLightPipeline, nullptr);
	vkDestroyPipelineLayout(m_renderer->GetDevice(), m_pointLightPipelineLayout, nullptr);

	// Destroy descriptors.
	vkDestroyDescriptorPool(m_renderer->GetDevice(), m_descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(m_renderer->GetDevice(), m_dirLightUBOLayout, nullptr);

	// Destroy UBOs
	vkDestroyBuffer(m_renderer->GetDevice(), m_dirLightUBO, nullptr);
	vkFreeMemory(m_renderer->GetDevice(), m_dirLightUBOMemory, nullptr);

	// Destroy vertex buffers.
	vkDestroyBuffer(m_renderer->GetDevice(), m_pointLightStageInsBuffer, nullptr);
	vkFreeMemory(m_renderer->GetDevice(), m_pointLightStageInsMemory, nullptr);

	vkDestroyBuffer(m_renderer->GetDevice(), m_pointLightInsBuffer, nullptr);
	vkFreeMemory(m_renderer->GetDevice(), m_pointLightInsMemory, nullptr);

	// Delete point light volume mesh.
	delete m_pointLightVolMesh;
	m_pointLightVolMesh = nullptr;
}

const VkDescriptorSetLayout& LightingManager::DirLightSetLayout() 
{
	return m_dirLightUBOLayout;
}

const VkDescriptorSet& LightingManager::DirLightUBOSet() 
{
	return m_dirLightUBOSet;
}

const VkPipeline& LightingManager::DirLightPipeline() 
{
	return m_dirLightPipeline;
}

const VkPipelineLayout& LightingManager::DirLightPipelineLayout() 
{
	return m_dirLightPipelineLayout;
}

const VkPipeline& LightingManager::PointLightPipeline()
{
	return m_pointLightPipeline;
}

const VkPipelineLayout& LightingManager::PointLightPipelineLayout()
{
	return m_pointLightPipelineLayout;
}

void LightingManager::UpdateDirLight(const DirectionalLight& data, const unsigned int& nIndex) 
{
	if (nIndex >= DIRECTIONAL_LIGHT_COUNT)
		return;

	// Copy new data to local buffer.
	m_dirLights[nIndex] = data;

	// Flag changes.
	m_bDirLightChange = true;
}

void LightingManager::AddPointLight(const PointLight& data) 
{
	// Update start of update region.
	m_nChangePLightStart = glm::min<unsigned int>(m_nChangePLightStart, m_pointLights.Count());

	m_pointLights.Push(data);

	// Update end of update region.
	m_nChangePLightEnd = m_pointLights.Count();

	m_bPointLightChange = true;
}

void LightingManager::UpdatePointLight(const PointLight& data, const unsigned int& nIndex)
{
	if (nIndex >= static_cast<unsigned int>(m_pointLights.Count()))
		return;

	// Update region.
	m_nChangePLightStart = glm::min<unsigned int>(m_nChangePLightStart, nIndex);
	m_nChangePLightEnd = glm::max<unsigned int>(m_nChangePLightEnd, nIndex + 1);

	// Copy new data to local buffer.
	m_pointLights[nIndex] = data;

	// Flag changes.
	m_bPointLightChange = true;
}

void LightingManager::RecreatePipelines(Shader* dirLightShader, Shader* pointLightShader, const unsigned int& nWindowWidth, const unsigned int& nWindowHeight)
{
	// Destroy pipelines.
	vkDestroyPipeline(m_renderer->GetDevice(), m_dirLightPipeline, nullptr);
	vkDestroyPipelineLayout(m_renderer->GetDevice(), m_dirLightPipelineLayout, nullptr);

	vkDestroyPipeline(m_renderer->GetDevice(), m_pointLightPipeline, nullptr);
	vkDestroyPipelineLayout(m_renderer->GetDevice(), m_pointLightPipelineLayout, nullptr);

	// Re-create pipelines.
	CreateDirLightingPipeline(dirLightShader, nWindowWidth, nWindowHeight);
	CreatePointLightingPipeline(pointLightShader, nWindowWidth, nWindowHeight);
}

void LightingManager::DrawPointLights(VkCommandBuffer& cmdBuffer)
{
	// Bind point light volume vertex buffer and point light instance buffer.
	m_pointLightVolMesh->Bind(cmdBuffer, m_pointLightInsBuffer);

	// Draw...
	vkCmdDrawIndexed(cmdBuffer, m_pointLightVolMesh->IndexCount(), MAX_POINT_LIGHT_COUNT, 0, 0, 0);
}

const bool& LightingManager::DirLightingChanged() 
{
	return m_bDirLightChange;
}

const bool& LightingManager::PointLightingChanged()
{
	return m_bPointLightChange;
}

void LightingManager::UpdateDirLights()
{
	int nBufferSize = sizeof(DirectionalLight) * DIRECTIONAL_LIGHT_COUNT;

	// Map directional light UBO memory...
	void* mappedPtr = nullptr;
	RENDERER_SAFECALL(vkMapMemory(m_renderer->GetDevice(), m_dirLightUBOMemory, 0, nBufferSize, 0, &mappedPtr), "Lighting Manager Error: Failed to map directional light data for update.");

	memcpy_s(mappedPtr, nBufferSize, m_dirLights.Data(), nBufferSize);

	// Unmap buffer memory.
	vkUnmapMemory(m_renderer->GetDevice(), m_dirLightUBOMemory);

	m_bDirLightChange = false;
}

void LightingManager::UpdatePointLights() 
{
	int nCopySize = sizeof(PointLight) * (m_nChangePLightEnd - m_nChangePLightStart);

	// Map instance staging buffer.
	void* bufferPtr = nullptr;
	RENDERER_SAFECALL(vkMapMemory(m_renderer->GetDevice(), m_pointLightStageInsMemory, sizeof(PointLight) * m_nChangePLightStart, nCopySize, 0, &bufferPtr), "Lighting Manager Error: Failed to update point light instance data on GPU.");

	// Copy data...
	memcpy_s(bufferPtr, nCopySize, &m_pointLights.Data()[m_nChangePLightStart], nCopySize);

	// Unmap buffer.
	vkUnmapMemory(m_renderer->GetDevice(), m_pointLightStageInsMemory);

	// Copy instance staging buffer to device local instance buffer.
	VkBufferCopy insCopyRegion = {};
	insCopyRegion.srcOffset = sizeof(PointLight) * m_nChangePLightStart;
	insCopyRegion.dstOffset = sizeof(PointLight) * m_nChangePLightStart;
	insCopyRegion.size = nCopySize;

	// Create and submit copy request.
	CopyRequest bufferCopyRequest = { m_pointLightStageInsBuffer, m_pointLightInsBuffer, insCopyRegion };
	m_renderer->RequestCopy(bufferCopyRequest);

	m_nChangePLightStart = 0U;
	m_nChangePLightEnd = 1U;

	m_bPointLightChange = false;
}

void LightingManager::CreatePointLightBuffers() 
{
	// Create point light staging buffer, with enough memory for MAX_POINT_LIGHT_COUNT lights.
	m_renderer->CreateBuffer(sizeof(PointLight) * MAX_POINT_LIGHT_COUNT, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, m_pointLightStageInsBuffer, m_pointLightStageInsMemory);

	// Create point light device local buffer, with enough memory for MAX_POINT_LIGHT_COUNT lights.
	m_renderer->CreateBuffer(sizeof(PointLight) * MAX_POINT_LIGHT_COUNT, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_pointLightInsBuffer, m_pointLightInsMemory);
}

void LightingManager::CreateDescriptorPool() 
{
	// Pool size for directional light uniform buffer.
	VkDescriptorPoolSize dirLightUBOPoolSize = {};
	dirLightUBOPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	dirLightUBOPoolSize.descriptorCount = 1;

	VkDescriptorPoolCreateInfo poolCreateInfo = {};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCreateInfo.maxSets = dirLightUBOPoolSize.descriptorCount;
	poolCreateInfo.poolSizeCount = 1;
	poolCreateInfo.pPoolSizes = &dirLightUBOPoolSize;
	poolCreateInfo.flags = 0;
	poolCreateInfo.pNext = nullptr;

	RENDERER_SAFECALL(vkCreateDescriptorPool(m_renderer->GetDevice(), &poolCreateInfo, nullptr, &m_descriptorPool), "Lighting Manager Error: Failed to create descriptor pool.");
}

void LightingManager::CreateSetLayouts() 
{
	VkDescriptorSetLayoutBinding dirLightUBOLayoutInfo = {};
	dirLightUBOLayoutInfo.binding = 0;
	dirLightUBOLayoutInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	dirLightUBOLayoutInfo.descriptorCount = 1;
	dirLightUBOLayoutInfo.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.flags = 0;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &dirLightUBOLayoutInfo;
	layoutInfo.pNext = nullptr;

	RENDERER_SAFECALL(vkCreateDescriptorSetLayout(m_renderer->GetDevice(), &layoutInfo, nullptr, &m_dirLightUBOLayout), "Lighting Manager Error: Failed to create directional light UBO set layout.");
}

void LightingManager::CreateDescriptorSets() 
{
	VkDescriptorSetAllocateInfo dirLightUBOAllocInfo = {};
	dirLightUBOAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	dirLightUBOAllocInfo.pSetLayouts = &m_dirLightUBOLayout;
	dirLightUBOAllocInfo.descriptorPool = m_descriptorPool;
	dirLightUBOAllocInfo.descriptorSetCount = 1;
	dirLightUBOAllocInfo.pNext = nullptr;

	// Allocate directional light UBO descriptor set.
	RENDERER_SAFECALL(vkAllocateDescriptorSets(m_renderer->GetDevice(), &dirLightUBOAllocInfo, &m_dirLightUBOSet), "Lighting Manager Error: Failed to allocate directional light UBO descriptor set.");

	// Write UBO to descriptor set.

	// Information about the directional light UBO.
	VkDescriptorBufferInfo dirLightUBOInfo = {};
	dirLightUBOInfo.buffer = m_dirLightUBO;
	dirLightUBOInfo.offset = 0;
	dirLightUBOInfo.range = sizeof(DirectionalLight) * DIRECTIONAL_LIGHT_COUNT;

	// Information to write to the descriptor set.
	VkWriteDescriptorSet dirLightUBOWrite = {};
	dirLightUBOWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	dirLightUBOWrite.descriptorCount = 1;
	dirLightUBOWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	dirLightUBOWrite.dstArrayElement = 0;
	dirLightUBOWrite.dstBinding = 0;
	dirLightUBOWrite.dstSet = m_dirLightUBOSet;
	dirLightUBOWrite.pBufferInfo = &dirLightUBOInfo;
	dirLightUBOWrite.pNext = nullptr;

	// Update directional light UBO set with UBO information.
	vkUpdateDescriptorSets(m_renderer->GetDevice(), 1, &dirLightUBOWrite, 0, nullptr);
}

void LightingManager::CreateDirLightingPipeline(Shader* dirLightShader, const unsigned int& nWindowWidth, const unsigned int& nWindowHeight)
{
	// Vertex shader stage information.
	VkPipelineShaderStageCreateInfo vertStageInfo = {};
	vertStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertStageInfo.module = dirLightShader->m_vertModule;
	vertStageInfo.pName = "main";

	// Fragment shader stage information.
	VkPipelineShaderStageCreateInfo fragStageInfo = {};
	fragStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragStageInfo.module = dirLightShader->m_fragModule;
	fragStageInfo.pName = "main";

	// Array of shader stage information.
	VkPipelineShaderStageCreateInfo shaderStageInfos[] = { vertStageInfo, fragStageInfo };

	VkPipelineVertexInputStateCreateInfo vertInputInfo = {};
	vertInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertInputInfo.vertexBindingDescriptionCount = 0;
	vertInputInfo.pVertexBindingDescriptions = nullptr;
	vertInputInfo.vertexAttributeDescriptionCount = 0;
	vertInputInfo.pVertexAttributeDescriptions = nullptr;

	// Input assembly stage configuration.
	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	// Viewport configuration.
	VkViewport viewPort = {};
	viewPort.x = 0.0f;
	viewPort.y = 0.0f;
	viewPort.width = (float)nWindowWidth;
	viewPort.height = (float)nWindowHeight;
	viewPort.minDepth = 0.0f;
	viewPort.maxDepth = 1.0f;

	// Scissor configuration.
	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = { nWindowWidth, nWindowHeight };

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
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

	// Used for shadow mapping...
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f;
	rasterizer.depthBiasClamp = 0.0f;
	rasterizer.depthBiasSlopeFactor = 0.0f;

	// Depth / Stencil state
	VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
	depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencilState.depthTestEnable = VK_FALSE; // No depth testing for deferred shading lighting pass.
	depthStencilState.depthWriteEnable = VK_FALSE; // Also no writing to depth.
	depthStencilState.stencilTestEnable = VK_FALSE; // Not needed.
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

	// TODO: Set this up for deferred shading, to allow blending of multiple lights.
	// Color blending
	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE; // Blending not required yet...
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendAttachmentState colorBlendAttachments[] = { colorBlendAttachment };

	VkPipelineColorBlendStateCreateInfo colorBlending = {};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1; // Blending for color output.
	colorBlending.pAttachments = colorBlendAttachments;
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	VkDescriptorSetLayout setLayouts[] = { m_renderer->MVPUBOSetLayout(), m_renderer->GBufferInputSetLayout(), m_dirLightUBOLayout };

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 3;
	pipelineLayoutInfo.pSetLayouts = setLayouts; // Lighting pass descriptor sets...
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	RENDERER_SAFECALL(vkCreatePipelineLayout(m_renderer->GetDevice(), &pipelineLayoutInfo, nullptr, &m_dirLightPipelineLayout), "Renderer Error: Failed to create lighting graphics pipeline layout.");

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
	pipelineInfo.layout = m_dirLightPipelineLayout;
	pipelineInfo.renderPass = m_renderer->MainRenderPass();
	pipelineInfo.subpass = POST_SUBPASS_INDEX;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	RENDERER_SAFECALL(vkCreateGraphicsPipelines(m_renderer->GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_dirLightPipeline), "Renderer Error: Failed to create lighting graphics pipeline.");
}

inline void LightingManager::CreatePointLightingPipeline(Shader* pLightShader, const unsigned int& nWindowWidth, const unsigned int& nWindowHeight) 
{
	// Vertex shader stage information.
	VkPipelineShaderStageCreateInfo vertStageInfo = {};
	vertStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertStageInfo.module = pLightShader->m_vertModule;
	vertStageInfo.pName = "main";

	// Fragment shader stage information.
	VkPipelineShaderStageCreateInfo fragStageInfo = {};
	fragStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragStageInfo.module = pLightShader->m_fragModule;
	fragStageInfo.pName = "main";

	// Array of shader stage information.
	VkPipelineShaderStageCreateInfo shaderStageInfos[] = { vertStageInfo, fragStageInfo };

	VkVertexInputBindingDescription pointLightInsDesc = {};
	pointLightInsDesc.binding = 1;
	pointLightInsDesc.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
	pointLightInsDesc.stride = sizeof(PointLight);

	const VertexInfo* pointLightMeshFormat = m_pointLightVolMesh->VertexFormat();

	// Instance format.
	VertexInfo insInfo({ VERTEX_ATTRIB_FLOAT4, VERTEX_ATTRIB_FLOAT4 }, true, pointLightMeshFormat);

	// Create attribute description array and copy the mesh attributes into it.
	DynamicArray<VkVertexInputAttributeDescription> attrDescriptions(pointLightMeshFormat->AttributeDescriptionCount() + 2, 1);
	memcpy_s(attrDescriptions.Data(), attrDescriptions.GetSize() * sizeof(VkVertexInputAttributeDescription), 
		pointLightMeshFormat->AttributeDescriptions(), pointLightMeshFormat->AttributeDescriptionCount() * sizeof(VkVertexInputAttributeDescription));
	
	attrDescriptions.SetCount(pointLightMeshFormat->AttributeDescriptionCount());

	// Append point light attribute descriptions.
	for (int i = 0; i < insInfo.AttributeDescriptionCount(); ++i)
		attrDescriptions.Push(insInfo.AttributeDescriptions()[i]);

	VkVertexInputBindingDescription bindingDescs[] = { pointLightMeshFormat->BindingDescription(), insInfo.BindingDescription() };

	VkPipelineVertexInputStateCreateInfo vertInputInfo = {};
	vertInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertInputInfo.vertexBindingDescriptionCount = 2;
	vertInputInfo.pVertexBindingDescriptions = bindingDescs;
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
	viewPort.width = (float)nWindowWidth;
	viewPort.height = (float)nWindowHeight;
	viewPort.minDepth = 0.0f;
	viewPort.maxDepth = 1.0f;

	// Scissor configuration.
	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = { nWindowWidth, nWindowHeight };

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
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

	// Used for shadow mapping...
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f;
	rasterizer.depthBiasClamp = 0.0f;
	rasterizer.depthBiasSlopeFactor = 0.0f;

	// Depth / Stencil state
	VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
	depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencilState.depthTestEnable = VK_FALSE; // No depth testing for deferred shading lighting pass.
	depthStencilState.depthWriteEnable = VK_FALSE; // Also no writing to depth.
	depthStencilState.stencilTestEnable = VK_FALSE; // Not needed.
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

	// TODO: Set this up for deferred shading, to allow blending of multiple lights.
	// Color blending
	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.blendEnable = VK_TRUE; // Blending is required to combine the colors and intensity of overlapping lights.
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // The color and intensity of lights will be combined.
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendAttachmentState colorBlendAttachments[] = { colorBlendAttachment };

	VkPipelineColorBlendStateCreateInfo colorBlending = {};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1; // Blending for color output.
	colorBlending.pAttachments = colorBlendAttachments;
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	VkDescriptorSetLayout setLayouts[] = { m_renderer->MVPUBOSetLayout(), m_renderer->GBufferInputSetLayout() };

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 2;
	pipelineLayoutInfo.pSetLayouts = setLayouts; // Lighting pass descriptor sets...
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	RENDERER_SAFECALL(vkCreatePipelineLayout(m_renderer->GetDevice(), &pipelineLayoutInfo, nullptr, &m_pointLightPipelineLayout), "Renderer Error: Failed to create lighting graphics pipeline layout.");

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
	pipelineInfo.layout = m_dirLightPipelineLayout;
	pipelineInfo.renderPass = m_renderer->MainRenderPass();
	pipelineInfo.subpass = POST_SUBPASS_INDEX;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	RENDERER_SAFECALL(vkCreateGraphicsPipelines(m_renderer->GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pointLightPipeline), "Renderer Error: Failed to create lighting graphics pipeline.");
}