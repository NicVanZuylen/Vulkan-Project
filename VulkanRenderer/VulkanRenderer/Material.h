#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include "DynamicArray.h"
#include "Table.h"
#include "Texture.h"
#include <initializer_list>

struct Shader;

class Sampler;

class Material
{
public:

	/*
	Constructor:
	Param:
	    Renderer* renderer: The renderer that will use this material. 
		Shader* shader: The shader used by this material.
		const DynamicArray<Texture*> textureMaps: Array of textures used by this material.
		bool bUseMVPUBO: Whether or not to use the MVP matrix uniform buffer in the shader.
	*/
	Material(Renderer* renderer, Shader* shader, const DynamicArray<Texture*>& textureMaps, bool bUseMVPUBO = true);

	/*
	Constructor:
	Param:
		Renderer* renderer: The renderer that will use this material.
		Shader* shader: The shader used by this material.
		const std::initializer_list<Texture*>& textureMaps: Array of textures used by this material.
		bool bUseMVPUBO: Whether or not to use the MVP matrix uniform buffer in the shader.
	*/
	Material(Renderer* renderer, Shader* shader, const std::initializer_list<Texture*>& textureMaps, bool bUseMVPUBO = true);

	~Material();

	/*
	Description: Issue vulkan command for using the specified descriptor set of this material. (Specified by index.)
	Param:
	    VkCommandBuffer& cmdBuffer: The vulkan command buffer to issue commands to.
		VkPipelineLayout& pipeline: The pipeline to bind this material's descriptor sets to.
		VkDescriptorSet& mvpUBOSet: The MVP matrix UBO descriptor set to bind for this frame.
		const unsigned int& nBufferIndex: The index of the swap chain image.
	*/
	void UseDescriptorSet(VkCommandBuffer& cmdBuffer, VkPipelineLayout& pipeline, VkDescriptorSet& mvpUBOSet, const unsigned int& nBufferIndex);

	/*
	Description: Set the texture sampler used by this material.
	Param:
	    Sampler* sampler: The sampler to use for texture sampling in shaders.
	*/
	void SetSampler(Sampler* sampler);

	/*
	Description: Get a reference to the shader used by this material.
	*/
	const Shader* GetShader() const;

	/*
	Description: Get the name identifier of this material.
	*/
	const std::string& GetName() const;

	/*
	Description: The descriptor set layout of this material.
	*/
	const VkDescriptorSetLayout& GetDescriptorLayout() const;

private:

	/*
	Description: Add a texture map to this material, to be used in shaders.
	Param:
	    Texture* texture: The texture to add to the material.
	*/
	void AddTextureMap(Texture* texture);

	// Create descriptor set layouts.
	inline void CreateDescriptorSetLayouts();

	// Create the descriptor pool for the material.
	void CreateDescriptorObjects();

	// Update descriptor sets.
	void UpdateDescriptorSets();

	static Sampler* m_defaultSampler; // Used if no sampler is explicitly provided.
	static int m_globalMaterialCount; // Tracks the amount of existing materials, if there is none the default sampler is freed when the last material is destroyed.

	Renderer* m_renderer;
	Shader* m_shader;
	Sampler* m_sampler;
	DynamicArray<Texture*> m_textures;
	bool m_bUseMVPUBO; // Flags the use of the MVP matrix UBO for this material.

	VkDescriptorPool m_descriptorPool;
	VkDescriptorSetLayout m_descriptorSetLayout;
	DynamicArray<VkDescriptorSet> m_descriptorSets;

	std::string m_nameID; // Unique identifier for this material, based upon the shader and textures used.
};

