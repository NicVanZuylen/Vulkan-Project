#include "ShadowMap.h"
#include "Shader.h"
#include "Texture.h"
#include "Mesh.h"
#include "SubScene.h"
#include "RenderObject.h"
#include "Material.h"
#include "glm/include/gtc/matrix_transform.hpp"

VkCommandBufferInheritanceInfo ShadowMap::m_inheritanceInfo =
{
	VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
	nullptr,
	VK_NULL_HANDLE,
	SHADOW_MAPPING_SUBPASS_INDEX,
	VK_NULL_HANDLE,
	VK_FALSE,
	0,
	0
};

VkCommandBufferBeginInfo ShadowMap::m_beginInfo =
{
	VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
	nullptr,
	VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT | VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
	&ShadowMap::m_inheritanceInfo
};

ShadowMap::ShadowMap(Renderer* renderer, Texture* shadowMap, uint32_t nWidth, uint32_t nHeight, DynamicArray<PipelineData*>* pipelines, VkCommandPool cmdPool, VkRenderPass pass, uint32_t nQueueFamilyIndex) : RenderModule(renderer, cmdPool, pass, nQueueFamilyIndex, false)
{
	m_shadowMap = shadowMap;

	m_nShadowMapWidth = nWidth;
	m_nShadowMapHeight = nHeight;

	m_pipelines = pipelines;

	m_nTransferCamera = MAX_FRAMES_IN_FLIGHT;

	m_vertShader = new Shader(renderer, SHADOW_MAPPING_VERT_SHADER_PATH, "");

	// Create resources...
	CreateShadowMapCamera();

	// Create descriptors...
	CreateDescriptorPool();
	CreateDescriptorSetLayouts();
	CreateDescriptorSets();
	UpdateDescriptorSets();

	// Create shadow mapping render pipeline.
	CreateRenderPipeline();
}

ShadowMap::~ShadowMap()
{
	VkDevice device = m_renderer->GetDevice();

	// Destroy render pipeline.
	vkDestroyPipelineLayout(device, m_shadowMapPipelineLayout, nullptr);
	vkDestroyPipeline(device, m_shadowMapPipeline, nullptr);

	// Destroy descriptors...
	vkDestroyDescriptorPool(device, m_descPool, nullptr);
	vkDestroyDescriptorSetLayout(device, m_camSetLayout, nullptr);

	// Free resources...

	for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) 
	{
		// Destroy staging buffers...
		vkDestroyBuffer(device, m_shadowCamStagingBufs[i], nullptr);
		vkFreeMemory(device, m_shadowCamStagingMemories[i], nullptr);

		// Destroy device local buffers...
		vkDestroyBuffer(device, m_shadowCamUBOs[i], nullptr);
		vkFreeMemory(device, m_shadowCamMemories[i], nullptr);
	}

	delete m_vertShader;
}

void ShadowMap::RecordCommandBuffer(const uint32_t& nPresentImageIndex, const uint32_t& nFrameIndex, const VkFramebuffer& framebuffer, const VkCommandBuffer transferCmdBuf)
{
	// Set inheritence framebuffer & render pass.
	m_inheritanceInfo.framebuffer = framebuffer;
	m_inheritanceInfo.renderPass = m_renderPass;

	VkCommandBuffer cmdBuf = m_cmdBuffers[nFrameIndex];

	// Update camera UBO if needed.
	if(m_nTransferCamera) 
	{
		// Map staging buffer & update it's contents.
		void* ptr = nullptr;
		RENDERER_SAFECALL(vkMapMemory(m_renderer->GetDevice(), m_shadowCamStagingMemories[nFrameIndex], 0, VK_WHOLE_SIZE, 0, &ptr), "Shadow Map Error: Failed to map camera memory for updating.");

		// Copy camera data to staging buffer.
		std::memcpy(ptr, &m_camera, sizeof(ShadowMapCamera));

		vkUnmapMemory(m_renderer->GetDevice(), m_shadowCamStagingMemories[nFrameIndex]);

		VkBufferCopy copyRegion = { 0, 0, sizeof(ShadowMapCamera) };
		vkCmdCopyBuffer(transferCmdBuf, m_shadowCamStagingBufs[nFrameIndex], m_shadowCamUBOs[nFrameIndex], 1, &copyRegion);

		m_nTransferCamera -= 1;
	}

	// Begin recording...
	RENDERER_SAFECALL(vkBeginCommandBuffer(cmdBuf, &m_beginInfo), "GBufferPass Error: Failed to begin recording of draw commands.");

	DynamicArray<PipelineData*>& pipelines = *m_pipelines;

	// Only one pipeline & descriptor set needs to be bound as the same is used for all when rendering a shadow map.
	vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowMapPipeline);
	vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowMapPipelineLayout, 0, 1, &m_camDescSets[nFrameIndex], 0, nullptr);

	// Iterate through all pipelines for the subscene and draw their renderobjects.
	for (uint32_t i = 0; i < pipelines.Count(); ++i)
	{
		PipelineData& data = *pipelines[i];

		for (uint32_t j = 0; j < data.m_renderObjects.Count(); ++j)
		{
			RenderObject& obj = *data.m_renderObjects[j];

			// Draw object into shadow map.
			obj.CommandDraw(cmdBuf);
		}
	}

	// End recording...
	RENDERER_SAFECALL(vkEndCommandBuffer(cmdBuf), "GBufferPass Error: Failed to end recording of draw commands.");
}

void ShadowMap::UpdateCamera(glm::vec4 v4LookDirection)
{
	glm::vec3 shadowCamPos = -v4LookDirection * 100.0f;

	// Set shadow map camera view matrix to the inverse of a matrix looking at the world origin, from where the look direction is coming from.
	m_camera.m_viewMat = glm::inverse(glm::lookAt(shadowCamPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
	//m_camera.m_viewMat = glm::inverse(glm::mat4());

	// Change coordinate system using this matrix.
	glm::mat4 axisCorrection; // Inverts the Y axis to match the OpenGL coordinate system.
	axisCorrection[1][1] = -1.0f;
	axisCorrection[2][2] = 1.0f;
	axisCorrection[3][3] = 1.0f;

	// Set camera projection matrix.
	//m_camera.m_projMat = glm::ortho(0.0f, static_cast<float>(m_nShadowMapWidth), 0.0f, static_cast<float>(m_nShadowMapHeight), 1.0f, 1000.0f);

	// Numbers define the bounding box of the scene visible in the shadow map.
	m_camera.m_projMat = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, -10.0f, 10.0f);
	//m_camera.m_projMat = glm::perspective(glm::radians(45.0f), static_cast<float>(m_nShadowMapWidth / m_nShadowMapHeight), 0.01f, 1000.0f);

	m_nTransferCamera = MAX_FRAMES_IN_FLIGHT;
}

Texture* ShadowMap::GetShadowMapImage()
{
	return m_shadowMap;
}

inline void ShadowMap::CreateShadowMapCamera()
{
	for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) 
	{
		// Create staging buffers...
		m_renderer->CreateBuffer(sizeof(ShadowMapCamera), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_shadowCamStagingBufs[i], m_shadowCamStagingMemories[i]);

		// Create device local buffers...
	    m_renderer->CreateBuffer(sizeof(ShadowMapCamera), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_shadowCamUBOs[i], m_shadowCamMemories[i]);
	}
}

inline void ShadowMap::CreateDescriptorPool()
{
	VkDescriptorPoolSize camUBOPoolsize = {};
	camUBOPoolsize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	camUBOPoolsize.descriptorCount = MAX_FRAMES_IN_FLIGHT;

	VkDescriptorPoolCreateInfo descPoolCreateInfo = {};
	descPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descPoolCreateInfo.pNext = nullptr;
	descPoolCreateInfo.poolSizeCount = 1;
	descPoolCreateInfo.pPoolSizes = &camUBOPoolsize;
	descPoolCreateInfo.flags = 0;
	descPoolCreateInfo.maxSets = MAX_FRAMES_IN_FLIGHT;

	RENDERER_SAFECALL(vkCreateDescriptorPool(m_renderer->GetDevice(), &descPoolCreateInfo, nullptr, &m_descPool), "Shadow Map Error: Failed to create descriptor pool.");
}

inline void ShadowMap::CreateDescriptorSetLayouts()
{
	VkDescriptorSetLayoutBinding camBinding = {};
	camBinding.binding = 0;
	camBinding.descriptorCount = 1;
	camBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	camBinding.pImmutableSamplers = nullptr;
	camBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; // Only needs to be used for transforming vertices for rendering the shadow map.

	VkDescriptorSetLayoutCreateInfo camSetLayoutInfo = {};
	camSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	camSetLayoutInfo.pNext = nullptr;
	camSetLayoutInfo.bindingCount = 1;
	camSetLayoutInfo.pBindings = &camBinding;
	camSetLayoutInfo.flags = 0;

	RENDERER_SAFECALL(vkCreateDescriptorSetLayout(m_renderer->GetDevice(), &camSetLayoutInfo, nullptr, &m_camSetLayout), "Shadow Map Error: Failed to create shadow map camera descriptor set layout.");
}

inline void ShadowMap::CreateDescriptorSets()
{
	DynamicArray<VkDescriptorSetLayout> setLayouts(MAX_FRAMES_IN_FLIGHT, 1);
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		setLayouts.Push(m_camSetLayout);

	VkDescriptorSetAllocateInfo camSetAllocInfo = {};
	camSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	camSetAllocInfo.pNext = nullptr;
	camSetAllocInfo.descriptorPool = m_descPool;
	camSetAllocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
	camSetAllocInfo.pSetLayouts = setLayouts.Data();

	RENDERER_SAFECALL(vkAllocateDescriptorSets(m_renderer->GetDevice(), &camSetAllocInfo, m_camDescSets), "Shadow Map Error: Failed to allocate camera descriptor sets.");
}

inline void ShadowMap::UpdateDescriptorSets()
{
	VkDescriptorBufferInfo camBufferInfos[MAX_FRAMES_IN_FLIGHT];
	VkWriteDescriptorSet writes[MAX_FRAMES_IN_FLIGHT];

	for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) 
	{
		// Update buffer infos.
		camBufferInfos[i].buffer = m_shadowCamUBOs[i];
		camBufferInfos[i].offset = 0;
		camBufferInfos[i].range = VK_WHOLE_SIZE;

		// Set write structure data.
		writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[i].pNext = nullptr;
		writes[i].dstSet = m_camDescSets[i];
		writes[i].dstBinding = 0;
		writes[i].dstArrayElement = 0;
		writes[i].descriptorCount = 1; // 1 per set.
		writes[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writes[i].pBufferInfo = &camBufferInfos[i];
	}

	// Update descriptors.
	vkUpdateDescriptorSets(m_renderer->GetDevice(), MAX_FRAMES_IN_FLIGHT, writes, 0, nullptr);
}

inline void ShadowMap::CreateRenderPipeline()
{
	// Vertex shader stage information.
	VkPipelineShaderStageCreateInfo vertStageInfo = {};
	vertStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertStageInfo.module = m_vertShader->m_vertModule;
	vertStageInfo.pName = "main";

	VertexInfo vertInfo({ VERTEX_ATTRIB_FLOAT4, VERTEX_ATTRIB_FLOAT4, VERTEX_ATTRIB_FLOAT4, VERTEX_ATTRIB_FLOAT2 }, false, nullptr); // Complex vertex info.
	VertexInfo insInfo({ VERTEX_ATTRIB_FLOAT4, VERTEX_ATTRIB_FLOAT4, VERTEX_ATTRIB_FLOAT4, VERTEX_ATTRIB_FLOAT4 }, true, &vertInfo); // Instance info, only contains a model matrix.

	// Vertex binding descriptions.
	VkVertexInputBindingDescription bindingDescs[] = { vertInfo.BindingDescription(), insInfo.BindingDescription() };

	// Vertex attribute descriptions.
	int nDescCount = vertInfo.AttributeDescriptionCount() + insInfo.AttributeDescriptionCount();
	VkVertexInputAttributeDescription* attrDescriptions = new VkVertexInputAttributeDescription[nDescCount];

	// Copy vertex descriptions...
	std::memcpy(attrDescriptions, vertInfo.AttributeDescriptions(), sizeof(VkVertexInputAttributeDescription) * vertInfo.AttributeDescriptionCount());

	// Copy instance descriptions...
	std::memcpy(&attrDescriptions[vertInfo.AttributeDescriptionCount()], insInfo.AttributeDescriptions(), sizeof(VkVertexInputAttributeDescription) * insInfo.AttributeDescriptionCount());

	VkPipelineVertexInputStateCreateInfo vertInputInfo = {};
	vertInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertInputInfo.vertexBindingDescriptionCount = 2;
	vertInputInfo.pVertexBindingDescriptions = bindingDescs;
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
	viewPort.width = static_cast<float>(m_nShadowMapWidth);
	viewPort.height = static_cast<float>(m_nShadowMapHeight);
	viewPort.minDepth = 0.0f;
	viewPort.maxDepth = 1.0f;

	// Scissor configuration.
	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = { m_nShadowMapWidth, m_nShadowMapHeight };

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
	depthStencilState.depthCompareOp = VK_COMPARE_OP_ALWAYS;
	depthStencilState.depthTestEnable = VK_TRUE;
	depthStencilState.depthWriteEnable = VK_TRUE;
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

	// Color blending
	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.blendEnable = VK_FALSE; // Blending is not needed for shadow mapping.
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
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

	VkDescriptorSetLayout setLayouts[] = { m_camSetLayout };

	// Create pipeline layout.
	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = setLayouts; // Camera descriptor set.
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	RENDERER_SAFECALL(vkCreatePipelineLayout(m_renderer->GetDevice(), &pipelineLayoutInfo, nullptr, &m_shadowMapPipelineLayout), "Renderer Error: Failed to create lighting graphics pipeline layout.");

	// Create pipeline.
	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 1;
	pipelineInfo.pStages = &vertStageInfo;
	pipelineInfo.pVertexInputState = &vertInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampler;
	pipelineInfo.pDepthStencilState = &depthStencilState;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = nullptr;
	pipelineInfo.layout = m_shadowMapPipelineLayout;
	pipelineInfo.renderPass = m_renderPass;
	pipelineInfo.subpass = SHADOW_MAPPING_SUBPASS_INDEX;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	RENDERER_SAFECALL(vkCreateGraphicsPipelines(m_renderer->GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_shadowMapPipeline), "Renderer Error: Failed to create lighting graphics pipeline.");

	delete[] attrDescriptions;
}
