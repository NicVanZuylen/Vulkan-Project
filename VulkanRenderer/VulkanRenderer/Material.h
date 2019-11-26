#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <map>
#include "DynamicArray.h"
#include "Table.h"
#include "Texture.h"
#include <initializer_list>

#define MATERIAL_PROPERTY_BINDING 0
#define TEXTURE_MAP_BINDING 1

struct Shader;

class Sampler;

enum EMatPropType 
{
	MATERIAL_PROPERTY_FLOAT,
	MATERIAL_PROPERTY_FLOAT2,
	MATERIAL_PROPERTY_FLOAT3,
	MATERIAL_PROPERTY_FLOAT4
};

struct MaterialProperty 
{
	EMatPropType type;
	std::string name;
};

class Material
{
public:

	/*
	Constructor:
	Param:
	    Renderer* renderer: The renderer that will use this material. 
		Shader* shader: The shader used by this material.
		const DynamicArray<Texture*> textureMaps: Array of textures used by this material.
		const DynamicArray<MaterialProperty>& properties: Material properties to add to this material.
		bool bUseMVPUBO: Whether or not to use the MVP matrix uniform buffer in the shader.
	*/
	Material(Renderer* renderer, Shader* shader, const DynamicArray<Texture*>& textureMaps, const DynamicArray<MaterialProperty>& properties, bool bUseMVPUBO = true);

	/*
	Constructor:
	Param:
		Renderer* renderer: The renderer that will use this material.
		Shader* shader: The shader used by this material.
		const std::initializer_list<Texture*>& textureMaps: Array of textures used by this material.
		const std::initializer_list<MaterialProperty>& properties: Material properties to add to this material.
		bool bUseMVPUBO: Whether or not to use the MVP matrix uniform buffer in the shader.
	*/
	Material(Renderer* renderer, Shader* shader, const std::initializer_list<Texture*>& textureMaps, const std::initializer_list<MaterialProperty>& properties, bool bUseMVPUBO = true);

	~Material();

	/*
	Description: Issue vulkan command for using the specified descriptor set of this material. (Specified by index.)
	Param:
	    const VkCommandBuffer& cmdBuffer: The command buffer to issue commands to.
		const VkCommandBuffer& transferCmdBuf: The command buffer to issue transfer commands to.
		VkPipelineLayout& pipeline: The pipeline to bind this material's descriptor sets to.
		VkDescriptorSet& mvpUBOSet: The MVP matrix UBO descriptor set to bind for this frame.
		const unsigned int& nFrameIndex: The index of the current frame-in-flight.
	*/
	void UseDescriptorSet(const VkCommandBuffer& cmdBuffer, const VkCommandBuffer& transferCmdBuf, VkPipelineLayout& pipeline, VkDescriptorSet& mvpUBOSet, const unsigned int& nFrameIndex);

	/*
	Description: Set the texture sampler used by this material.
	Param:
	    Sampler* sampler: The sampler to use for texture sampling in shaders.
	*/
	void SetSampler(Sampler* sampler);

	void SetFloat(const std::string& name, float fVal);
	void SetFloat2(const std::string& name, const float* fVal);
	void SetFloat3(const std::string& name, const float* fVal);
	void SetFloat4(const std::string& name, const float* fVal);

	float GetFloat(const std::string& name);
	glm::vec2 GetFloat2(const std::string& name);
	glm::vec3 GetFloat3(const std::string& name);
	glm::vec4 GetFloat4(const std::string& name);

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

	/*
	Description: Returns whether or not the material has any texture maps.
	Return Type: bool
	*/
	bool HasTextures();

private:

	/*
	Description: Add a texture map to this material, to be used in shaders.
	Param:
	    Texture* texture: The texture to add to the material.
	*/
	void AddTextureMap(Texture* texture);

	/*
    Description: Add a property to this material accessible in it's shaders.
	Param:
        EMatPropType type: The data type of the property to add.
        const std::string& name: The name of the material property.
    */
	void AddProperty(EMatPropType type, const std::string& name);

	// Create material property uniform buffer.
	inline void CreateMatPropertyUBO();

	// Create descriptor set layouts.
	inline void CreateDescriptorSetLayouts();

	// Create the descriptor pool for the material.
	void CreateDescriptorObjects();

	// Update descriptor sets.
	void UpdateDescriptorSets();

	static Sampler* m_defaultSampler; // Used if no sampler is explicitly provided.
	static int m_globalMaterialCount; // Tracks the amount of existing materials, if there is none the default sampler is freed when the last material is destroyed.

	// ---------------------------------------------------------------------------------
	// Main

	Renderer* m_renderer;
	Shader* m_shader;
	Sampler* m_sampler;
	DynamicArray<Texture*> m_textures;
	bool m_bUseMVPUBO; // Flags the use of the MVP matrix UBO for this material.
	bool m_bHasTextures;
	short m_nUpdateProperties; // Set to MAX_FRAMES_IN_FLIGHT whenever material properties are updated.

	// ---------------------------------------------------------------------------------
	// Properties

	DynamicArray<char> m_matPropData; // Material property UBO data.
	std::map<const char*, int> m_matPropSearchIndices; // Search indices for material properties.

	VkBuffer m_propStagingBuf[MAX_FRAMES_IN_FLIGHT];
	VkDeviceMemory m_propStagingMem[MAX_FRAMES_IN_FLIGHT];

	VkBuffer m_propertyUBO[MAX_FRAMES_IN_FLIGHT];
	VkDeviceMemory m_propUBOMemory[MAX_FRAMES_IN_FLIGHT];

	// ---------------------------------------------------------------------------------
	// Descriptors

	VkDescriptorPool m_descriptorPool;
	VkDescriptorSetLayout m_matSetLayout;
	VkDescriptorSet m_matDescSets[MAX_FRAMES_IN_FLIGHT];

	std::string m_nameID; // Unique identifier for this material, based upon the shader and textures used.
};

