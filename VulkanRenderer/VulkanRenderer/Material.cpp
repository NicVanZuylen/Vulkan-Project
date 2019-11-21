#include "Material.h"
#include "Sampler.h"
#include "Shader.h"
#include "Scene.h"

Sampler* Material::m_defaultSampler = nullptr;
int Material::m_globalMaterialCount = 0;

Material::Material(Renderer* renderer, Shader* shader, const DynamicArray<Texture*>& textureMaps, bool bUseMVPUBO)
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

	// Add all textures in the provided array.
	for (uint32_t i = 0; i < textureMaps.Count(); ++i)
		AddTextureMap(textureMaps[i]);

	CreateDescriptorObjects();
}

Material::Material(Renderer* renderer, Shader* shader, const std::initializer_list<Texture*>& textureMaps, bool bUseMVPUBO)
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

	// Add all textures in the provided array.
	for (uint32_t i = 0; i < m_textures.Count(); ++i)
		m_nameID += "|" + m_textures[i]->GetName();

	CreateDescriptorObjects();
}

void Material::UseDescriptorSet(VkCommandBuffer& cmdBuffer, VkPipelineLayout& pipeline, VkDescriptorSet& mvpUBOSet, const unsigned int& nBufferIndex)
{
	VkDescriptorSet sets[2] = { mvpUBOSet, m_descriptorSets[nBufferIndex] };

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
		vkDestroyDescriptorSetLayout(m_renderer->GetDevice(), m_descriptorSetLayout, nullptr);
		vkDestroyDescriptorPool(m_renderer->GetDevice(), m_descriptorPool, nullptr);
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
	return m_descriptorSetLayout;
}

void Material::CreateDescriptorSetLayouts() 
{
	VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
	samplerLayoutBinding.binding = 0;
	samplerLayoutBinding.descriptorCount = m_textures.Count();
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutCreateInfo.bindingCount = 1;
	layoutCreateInfo.pBindings = &samplerLayoutBinding;

	RENDERER_SAFECALL(vkCreateDescriptorSetLayout(m_renderer->GetDevice(), &layoutCreateInfo, nullptr, &m_descriptorSetLayout), "Material Error: Failed to create descriptor set layout.");
}

void Material::CreateDescriptorObjects() 
{
	VkDescriptorPoolSize poolSize;
	poolSize.descriptorCount = MAX_FRAMES_IN_FLIGHT * m_textures.Count();
	poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

	VkDescriptorPoolCreateInfo poolCreateInfo = {};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCreateInfo.poolSizeCount = 1;
	poolCreateInfo.pPoolSizes = &poolSize;
	poolCreateInfo.maxSets = MAX_FRAMES_IN_FLIGHT;
	
	RENDERER_SAFECALL(vkCreateDescriptorPool(m_renderer->GetDevice(), &poolCreateInfo, nullptr, &m_descriptorPool), "Material Error: Failed to create descriptor pool.");

	CreateDescriptorSetLayouts();

	DynamicArray<VkDescriptorSetLayout> setLayouts(MAX_FRAMES_IN_FLIGHT, 1);
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		setLayouts.Push(m_descriptorSetLayout);

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = m_descriptorPool;
	allocInfo.descriptorSetCount = setLayouts.Count();
	allocInfo.pSetLayouts = setLayouts.Data();
	allocInfo.pNext = nullptr;

	// There should be a descriptor set for each frame-in-flight.
	m_descriptorSets.SetSize(MAX_FRAMES_IN_FLIGHT);
	m_descriptorSets.SetCount(m_descriptorSets.GetSize());

	RENDERER_SAFECALL(vkAllocateDescriptorSets(m_renderer->GetDevice(), &allocInfo, m_descriptorSets.Data()), "Material Error: Failed to create descriptor sets.");

	// Next step.
	UpdateDescriptorSets();
}

void Material::UpdateDescriptorSets() 
{
	// Image buffer information data.
	DynamicArray<VkDescriptorImageInfo> imageInfos(m_textures.Count(), 1);

	for (uint32_t i = 0; i < m_textures.Count(); ++i) 
	{
		imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfos[i].imageView = m_textures[i]->ImageView();
		imageInfos[i].sampler = m_sampler->GetHandle();
	}

	for(uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) 
	{
		// Textures.
		VkWriteDescriptorSet texWrite = {};
		texWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		texWrite.descriptorCount = m_textures.Count();
		texWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		texWrite.dstArrayElement = 0;
		texWrite.dstBinding = 0;
		texWrite.dstSet = m_descriptorSets[i];
		texWrite.pImageInfo = imageInfos.Data();

		// Update descriptor sets.
		vkUpdateDescriptorSets(m_renderer->GetDevice(), 1, &texWrite, 0, nullptr);
	}
}