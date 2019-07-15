#include "Material.h"
#include "Sampler.h"
#include "Shader.h"

Sampler* Material::m_defaultSampler = nullptr;
int Material::m_globalMaterialCount = 0;

Material::Material(Renderer* renderer, Shader* shader, bool bUseMVPUBO = true)
{
	m_renderer = renderer;
	m_shader = shader;
	m_descriptorPool = nullptr;

	// Increment global material count.
	m_globalMaterialCount++;

	// If there is no default sampler, create one.
	if(!m_defaultSampler) 
	{
		// Create default sampler.
		m_defaultSampler = new Sampler(m_renderer);
	}

	m_sampler = m_defaultSampler;

	m_nameID += shader->m_name;
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

void Material::CreateDescriptorSetLayouts() 
{
	// This needs to be the same as the binding defined in the renderer.
	VkDescriptorSetLayoutBinding uboLayoutBinding = {};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	uboLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
	samplerLayoutBinding.binding = m_bUseMVPUBO; // 0 if not using MVP UBO, otherwise 1.
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding layoutBindings[2] = { uboLayoutBinding, samplerLayoutBinding };

	VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutCreateInfo.bindingCount = 1 + m_bUseMVPUBO;
	layoutCreateInfo.pBindings = &layoutBindings[m_bUseMVPUBO];

	RENDERER_SAFECALL(vkCreateDescriptorSetLayout(m_renderer->GetDevice(), &layoutCreateInfo, nullptr, &m_descriptorSetLayout), "Material Error: Failed to create descriptor set layout.");
}

void Material::CreateDescriptorPool() 
{
	VkDescriptorPoolSize poolSizes[2] = {};
	poolSizes[0].descriptorCount = m_renderer->SwapChainImageCount();
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[1].descriptorCount = m_renderer->SwapChainImageCount();
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

	int poolSizeCount = 1 + m_bUseMVPUBO;

	VkDescriptorPoolCreateInfo poolCreateInfo = {};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCreateInfo.poolSizeCount = poolSizeCount;
	poolCreateInfo.pPoolSizes = &poolSizes[m_bUseMVPUBO];
	poolCreateInfo.maxSets = m_renderer->SwapChainImageCount();
	
	RENDERER_SAFECALL(vkCreateDescriptorPool(m_renderer->GetDevice(), &poolCreateInfo, nullptr, &m_descriptorPool), "Material Error: Failed to create descriptor pool.");

	CreateDescriptorSetLayouts();

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = m_descriptorPool;
	allocInfo.descriptorSetCount = m_renderer->SwapChainImageCount();
	allocInfo.pSetLayouts = &m_descriptorSetLayout;
	
	m_descriptorSets.SetSize(1 + m_bUseMVPUBO);
	m_descriptorSets.SetCount(m_descriptorSets.GetSize());

	RENDERER_SAFECALL(vkAllocateDescriptorSets(m_renderer->GetDevice(), &allocInfo, m_descriptorSets.Data()), "Material Error: Failed to create descriptor sets.");
}