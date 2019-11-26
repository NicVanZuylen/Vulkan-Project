#include "Material.h"
#include "Sampler.h"
#include "Shader.h"
#include "Scene.h"

Sampler* Material::m_defaultSampler = nullptr;
int Material::m_globalMaterialCount = 0;

Material::Material(Renderer* renderer, Shader* shader, const DynamicArray<Texture*>& textureMaps, const DynamicArray<MaterialProperty>& properties, bool bUseMVPUBO)
{
	m_renderer = renderer;
	m_shader = shader;
	m_descriptorPool = nullptr;
	m_bUseMVPUBO = bUseMVPUBO;
	m_nameID.clear();

	// Increment global material count.
	m_globalMaterialCount++;

	// If there is no default sampler, create one.
	if(!m_defaultSampler) 
	{
		// Create default sampler.
		m_defaultSampler = new Sampler(m_renderer);
	}

	m_sampler = m_defaultSampler;

	m_nameID += "S:Default|";
	m_nameID += shader->m_name;

	m_bHasTextures = textureMaps.Count() > 0;

	// Add all textures in the provided array.
	for (uint32_t i = 0; i < textureMaps.Count(); ++i)
		AddTextureMap(textureMaps[i]);

	// Add default color tint property.
	AddProperty(MATERIAL_PROPERTY_FLOAT4, "_ColorTint");

	// Set default color tint color to white.
	float fDefaultColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	SetFloat4("_ColorTint", fDefaultColor);

	// Add all properties in the provided array.
	for (uint32_t i = 0; i < properties.Count(); ++i)
		AddProperty(properties[i].type, properties[i].name);

	m_nUpdateProperties = MAX_FRAMES_IN_FLIGHT;

	CreateMatPropertyUBO();
	CreateDescriptorObjects();
}

Material::Material(Renderer* renderer, Shader* shader, const std::initializer_list<Texture*>& textureMaps, const std::initializer_list<MaterialProperty>& properties, bool bUseMVPUBO)
{
	m_renderer = renderer;
	m_shader = shader;
	m_descriptorPool = nullptr;
	m_bUseMVPUBO = bUseMVPUBO;
	m_nameID.clear();

	// Increment global material count.
	m_globalMaterialCount++;

	// If there is no default sampler, create one.
	if (!m_defaultSampler)
	{
		// Create default sampler.
		m_defaultSampler = new Sampler(m_renderer);
	}

	m_sampler = m_defaultSampler;

	m_nameID += "S:Default|";
	m_nameID += shader->m_name;

	m_textures = textureMaps;
	m_bHasTextures = m_textures.Count() > 0;

	// Add all textures in the provided array.
	for (uint32_t i = 0; i < m_textures.Count(); ++i)
		m_nameID += "|" + m_textures[i]->GetName();

	// Add default color tint property.
	AddProperty(MATERIAL_PROPERTY_FLOAT4, "_ColorTint");

	// Set default color tint color to white.
	float fDefaultColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	SetFloat4("_ColorTint", fDefaultColor);

	// Add all material properties in the provided array.
	const MaterialProperty* propertyList = properties.begin();
	for (uint32_t i = 0; i < properties.size(); ++i) 
		AddProperty(propertyList[i].type, propertyList[i].name);

	m_nUpdateProperties = MAX_FRAMES_IN_FLIGHT;

	CreateMatPropertyUBO();
	CreateDescriptorObjects();
}

void Material::UseDescriptorSet(const VkCommandBuffer& cmdBuffer, const VkCommandBuffer& transferCmdBuf, VkPipelineLayout& pipeline, VkDescriptorSet& mvpUBOSet, const unsigned int& nFrameIndex)
{
	if(m_nUpdateProperties) 
	{
		VkDevice device = m_renderer->GetDevice();

		// Map and update property staging buffer.
		void* mappedPtr = nullptr;
		vkMapMemory(device, m_propStagingMem[nFrameIndex], 0, VK_WHOLE_SIZE, 0, &mappedPtr);

		// Copy data into staging buffer.
		std::memcpy(mappedPtr, m_matPropData.Data(), m_matPropData.GetSize());

		// Unmap memory.
		vkUnmapMemory(device, m_propStagingMem[nFrameIndex]);

		// Issue staging buffer to ubo copy command.
		VkBufferCopy copyData = { 0, 0, m_matPropData.GetSize() };
		vkCmdCopyBuffer(transferCmdBuf, m_propStagingBuf[nFrameIndex], m_propertyUBO[nFrameIndex], 1, &copyData);

		m_nUpdateProperties -= 1;
	}

	VkDescriptorSet sets[2] = { mvpUBOSet, m_matDescSets[nFrameIndex] };

	vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline, 0, 2, sets, 0, nullptr);
}

Material::~Material()
{
	// Free sampler if this is the final material using it.
	--m_globalMaterialCount;

	if(m_defaultSampler && m_globalMaterialCount <= 0) 
	{
		m_globalMaterialCount = 0;

		delete m_defaultSampler;
		m_defaultSampler = nullptr;
	}

	// Free descriptor memory.

	if (m_descriptorPool) 
	{
		vkDestroyDescriptorSetLayout(m_renderer->GetDevice(), m_matSetLayout, nullptr);
		vkDestroyDescriptorPool(m_renderer->GetDevice(), m_descriptorPool, nullptr);
	}

	// Destroy buffers.
	for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) 
	{
		// Destroy staging buffers.
		if (m_propStagingBuf[i])
		{
			vkDestroyBuffer(m_renderer->GetDevice(), m_propStagingBuf[i], nullptr);
			vkFreeMemory(m_renderer->GetDevice(), m_propStagingMem[i], nullptr);
		}

		// Destroy property buffers.
		if(m_propertyUBO[i]) 
		{
		    vkDestroyBuffer(m_renderer->GetDevice(), m_propertyUBO[i], nullptr);
		    vkFreeMemory(m_renderer->GetDevice(), m_propUBOMemory[i], nullptr);
		}
	}
}

void Material::SetSampler(Sampler* sampler) 
{
	// Remove old sampler name from name ID.
	size_t samplerNamePos = m_nameID.find_last_of("S:") + 2; // Offset of sampler name in string.

	m_nameID.erase(samplerNamePos, m_sampler->GetNameID().size());
	m_nameID.insert(samplerNamePos, sampler->GetNameID());

	m_sampler = sampler;
}

void Material::AddTextureMap(Texture* texture) 
{
	m_nameID += "|" + texture->GetName();

	m_textures.Push(texture);
}

void Material::AddProperty(EMatPropType type, const std::string& name)
{
	// Allocate memory for the property.
	m_matPropSearchIndices[name.c_str()] = m_matPropData.Count(); // Assign search index.

	// Allocate property memory.
	m_matPropData.SetSize(m_matPropData.GetSize() + ((type + 1) * sizeof(float)));
	m_matPropData.SetCount(m_matPropData.GetSize());
}

inline void Material::CreateMatPropertyUBO()
{
	// Create buffers for each frame-in-flight, one staging and one device local.
	for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) 
	{
		m_renderer->CreateBuffer(m_matPropData.GetSize(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, m_propStagingBuf[i], m_propStagingMem[i]);
		m_renderer->CreateBuffer(m_matPropData.GetSize(), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_propertyUBO[i], m_propUBOMemory[i]);
	}
}

void Material::SetFloat(const std::string& name, float fVal)
{
	int nDataIndex = m_matPropSearchIndices[name.c_str()];

	// Find value pointer.
	float* ptr = (float*)(&m_matPropData[nDataIndex]);
	*ptr = fVal; // Set value.

	m_nUpdateProperties = MAX_FRAMES_IN_FLIGHT;
}

void Material::SetFloat2(const std::string& name, const float* fVal)
{
	int nDataIndex = m_matPropSearchIndices[name.c_str()];

	// Find value pointer.
	char* ptr = &m_matPropData[nDataIndex];
	
	// Copy data.
	std::memcpy(ptr, fVal, 2 * sizeof(float));

	m_nUpdateProperties = MAX_FRAMES_IN_FLIGHT;
}

void Material::SetFloat3(const std::string& name, const float* fVal)
{
	int nDataIndex = m_matPropSearchIndices[name.c_str()];

	// Find value pointer.
	char* ptr = &m_matPropData[nDataIndex];

	// Copy data.
	std::memcpy(ptr, fVal, 3 * sizeof(float));

	m_nUpdateProperties = MAX_FRAMES_IN_FLIGHT;
}

void Material::SetFloat4(const std::string& name, const float* fVal)
{
	int nDataIndex = m_matPropSearchIndices[name.c_str()];

	// Find value pointer.
	char* ptr = &m_matPropData[nDataIndex];

	// Copy data.
	std::memcpy(ptr, fVal, 4 * sizeof(float));

	m_nUpdateProperties = MAX_FRAMES_IN_FLIGHT;
}

float Material::GetFloat(const std::string& name)
{
	int nDataIndex = m_matPropSearchIndices[name.c_str()];

	// Find value pointer.
	float* ptr = (float*)(&m_matPropData[nDataIndex]);
	return *ptr;
}

glm::vec2 Material::GetFloat2(const std::string& name)
{
	int nDataIndex = m_matPropSearchIndices[name.c_str()];

	// Find value pointer.
	glm::vec2* ptr = (glm::vec2*)(&m_matPropData[nDataIndex]);
	return *ptr;
}

glm::vec3 Material::GetFloat3(const std::string& name)
{
	int nDataIndex = m_matPropSearchIndices[name.c_str()];

	// Find value pointer.
	glm::vec3* ptr = (glm::vec3*)(&m_matPropData[nDataIndex]);
	return *ptr;
}

glm::vec4 Material::GetFloat4(const std::string& name)
{
	int nDataIndex = m_matPropSearchIndices[name.c_str()];

	// Find value pointer.
	glm::vec4* ptr = (glm::vec4*)(&m_matPropData[nDataIndex]);
	return *ptr;
}

const Shader* Material::GetShader() const
{
	return m_shader;
}

const std::string& Material::GetName() const 
{
	return m_nameID;
}

const VkDescriptorSetLayout& Material::GetDescriptorLayout() const 
{
	return m_matSetLayout;
}

bool Material::HasTextures()
{
	return m_textures.Count() > 0;
}

void Material::CreateDescriptorSetLayouts() 
{
	VkDescriptorSetLayoutBinding matPropBinding = {};
	matPropBinding.binding = MATERIAL_PROPERTY_BINDING;
	matPropBinding.descriptorCount = 1;
	matPropBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	matPropBinding.pImmutableSamplers = nullptr;
	matPropBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutBinding texLayoutBinding = {};
	texLayoutBinding.binding = TEXTURE_MAP_BINDING; // Binding 1 for texture maps.
	texLayoutBinding.descriptorCount = m_textures.Count();
	texLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	texLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	texLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding bindings[] = { matPropBinding, texLayoutBinding };

	VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutCreateInfo.bindingCount = 1 + m_bHasTextures;
	layoutCreateInfo.pBindings = bindings;

	RENDERER_SAFECALL(vkCreateDescriptorSetLayout(m_renderer->GetDevice(), &layoutCreateInfo, nullptr, &m_matSetLayout), "Material Error: Failed to create descriptor set layout.");
}

void Material::CreateDescriptorObjects() 
{
	// Pool size for material propety UBO.
	VkDescriptorPoolSize propPoolSize;
	propPoolSize.descriptorCount = MAX_FRAMES_IN_FLIGHT;
	propPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

	// Pool size for texture maps.
	VkDescriptorPoolSize texPoolSize;
	texPoolSize.descriptorCount = MAX_FRAMES_IN_FLIGHT * m_textures.Count();
	texPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

	VkDescriptorPoolSize poolSizes[] = { propPoolSize, texPoolSize };

	// Create info for the descriptor pool.
	VkDescriptorPoolCreateInfo poolCreateInfo = {};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCreateInfo.poolSizeCount = 1 + m_bHasTextures;
	poolCreateInfo.pPoolSizes = poolSizes;
	poolCreateInfo.maxSets = MAX_FRAMES_IN_FLIGHT;
	
	RENDERER_SAFECALL(vkCreateDescriptorPool(m_renderer->GetDevice(), &poolCreateInfo, nullptr, &m_descriptorPool), "Material Error: Failed to create descriptor pool.");

	// Create set layouts for the material descriptors.
	CreateDescriptorSetLayouts();

	// The same set layout will be used for each set. Make an array where all elements are the same set layout.
	DynamicArray<VkDescriptorSetLayout> setLayouts(MAX_FRAMES_IN_FLIGHT, 1);
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		setLayouts.Push(m_matSetLayout);

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = m_descriptorPool;
	allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
	allocInfo.pSetLayouts = setLayouts.Data();
	allocInfo.pNext = nullptr;

	RENDERER_SAFECALL(vkAllocateDescriptorSets(m_renderer->GetDevice(), &allocInfo, m_matDescSets), "Material Error: Failed to create descriptor sets.");

	// Next step.
	UpdateDescriptorSets();
}

void Material::UpdateDescriptorSets() 
{
	// Image buffer information.
	DynamicArray<VkDescriptorImageInfo> imageInfos(m_textures.Count(), 1);

	for (uint32_t i = 0; i < m_textures.Count(); ++i)
	{
		imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfos[i].imageView = m_textures[i]->ImageView();
		imageInfos[i].sampler = m_sampler->GetHandle();
	}

	// Property UBOs
	VkWriteDescriptorSet propWrites[MAX_FRAMES_IN_FLIGHT];
	VkDescriptorBufferInfo propBufferInfos[MAX_FRAMES_IN_FLIGHT];

	// Textures.
	VkWriteDescriptorSet texWrites[MAX_FRAMES_IN_FLIGHT];

	for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) 
	{
		VkWriteDescriptorSet& propWriteRef = propWrites[i];

		// Set buffer
		propBufferInfos[i].buffer = m_propertyUBO[i];
		propBufferInfos[i].offset = 0;
		propBufferInfos[i].range = VK_WHOLE_SIZE;

		propWriteRef.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		propWriteRef.pNext = nullptr;
		propWriteRef.descriptorCount = 1;
		propWriteRef.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		propWriteRef.dstArrayElement = 0;
		propWriteRef.dstBinding = MATERIAL_PROPERTY_BINDING;
		propWriteRef.dstSet = m_matDescSets[i];
		propWriteRef.pBufferInfo = &propBufferInfos[i];

		if(m_bHasTextures) 
		{
			VkWriteDescriptorSet& texWriteRef = texWrites[i];

			texWriteRef.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			texWriteRef.pNext = nullptr;
			texWriteRef.descriptorCount = m_textures.Count();
			texWriteRef.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			texWriteRef.dstArrayElement = 0;
			texWriteRef.dstBinding = TEXTURE_MAP_BINDING;
			texWriteRef.dstSet = m_matDescSets[i];
			texWriteRef.pImageInfo = imageInfos.Data();
		}
	}

	VkDevice device = m_renderer->GetDevice();

	// Update descriptor sets.
	vkUpdateDescriptorSets(device, MAX_FRAMES_IN_FLIGHT, propWrites, 0, nullptr);

	if(m_bHasTextures)
	    vkUpdateDescriptorSets(device, MAX_FRAMES_IN_FLIGHT, texWrites, 0, nullptr);
}