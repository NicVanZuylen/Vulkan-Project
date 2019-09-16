#pragma once
#include <vulkan/vulkan.h>
#include "glm.hpp"
#include "DynamicArray.h"
#include "Texture.h"

class Renderer;

struct DirectionalLight 
{
	glm::vec4 m_v4Direction;
	glm::vec4 m_v4Color;
};

#define DIRECTIONAL_LIGHT_COUNT 1

class LightingManager
{
public:

	LightingManager(Renderer* renderer, Shader* dirLightShader, const unsigned int& nWindowWidth, const unsigned int& nWindowHeight);

	~LightingManager();

	/*
	Description: Get the descriptor set layout for the directional light UBO.
	Return Type const VkDescriptorSetLayout&
	*/
	const VkDescriptorSetLayout& DirLightSetLayout();

	/*
	Description: Get the descriptor set for the directional light UBO.
	Return Type const VkDescriptorSet&
	*/
	const VkDescriptorSet& DirLightUBOSet();

	/*
	Description: Get the directional lighting graphics pipeline.
	Return Type const VkPipeline&
	*/
	const VkPipeline& DirLightPipeline();

	/*
	Description: Get the directional lighting graphics pipeline layout.
	Return Type const VkPipelineLayout&
	*/
	const VkPipelineLayout& DirLightPipelineLayout();

	/*
	Description: Update the data on a directional light.
	Param:
	    const DirectionalLight& data: The new data for the directional light.
		const unsigned int& nIndex: Index of the directional light to update.
	*/
    void UpdateDirLight(const DirectionalLight& data, const unsigned int& nIndex);

	/*
	Description: Get whether or not the directional lighting was changed since the last update.
	Return Type const bool&
	*/
	const bool& DirLightingChanged();

	/*
	Description: Update directional lighting data on the GPU.
	*/
	inline void UpdateDirLights();

	/*
	Description: Re-create lighting graphics pipelines.
	*/
	void RecreatePipelines(Shader* dirLightShader, const unsigned int& nWindowWidth, const unsigned int& nWindowHeight);

private:

	inline void CreateDescriptorPool();

	inline void CreateSetLayouts();

	inline void CreateDescriptorSets();

	inline void CreateDirLightingPipeline(Shader* dirLightShader, const unsigned int& nWindowWidth, const unsigned int& nWindowHeight);

	Renderer* m_renderer;

	// ---------------------------------------------------------------------------------
	// Lights

	DynamicArray<DirectionalLight> m_dirLights;
	bool m_bDirLightChange;

	// ---------------------------------------------------------------------------------
	// Buffers

	VkBuffer m_dirLightUBO;
	VkDeviceMemory m_dirLightUBOMemory;

	// ---------------------------------------------------------------------------------
	// Descriptors
	VkDescriptorPool m_descriptorPool;
	VkDescriptorSetLayout m_dirLightUBOLayout;
	VkDescriptorSet m_dirLightUBOSet;

	// ---------------------------------------------------------------------------------
	// Pipelines

	VkPipeline m_dirLightPipeline;
	VkPipelineLayout m_dirLightPipelineLayout;

	// ---------------------------------------------------------------------------------
};

