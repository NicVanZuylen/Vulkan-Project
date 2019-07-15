#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include "DynamicArray.h"
#include "Table.h"
#include "Texture.h"

struct Shader;

class Sampler;

class Material
{
public:

	Material(Renderer* renderer, Shader* shader, bool bUseMVPUBO = true);

	~Material();

	/*
	Description: Set the texture sampler used by this material.
	*/
	void SetSampler(Sampler* sampler);

	/*
	Description: Add a texture map to this material, to be used in shaders.
	*/
	void AddTextureMap(Texture* texture);

	/*
	Description: Get a reference to the shader used by this material.
	*/
	const Shader* GetShader() const;

	/*
	Description: Get the name identifier of this material.
	*/
	const std::string& GetName() const;

private:

	// Create descriptor set layouts.
	inline void CreateDescriptorSetLayouts();

	// Create the descriptor pool for the material.
	void CreateDescriptorPool();

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

