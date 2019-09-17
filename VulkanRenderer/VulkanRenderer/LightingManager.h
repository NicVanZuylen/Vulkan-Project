#pragma once
#include <vulkan/vulkan.h>
#include "glm.hpp"
#include "DynamicArray.h"
#include "Texture.h"

class Renderer;
class Mesh;

struct DirectionalLight 
{
	glm::vec4 m_v4Direction;
	glm::vec4 m_v4Color;
};

struct PointLight 
{
	glm::vec4 m_v4Position;
	glm::vec3 m_v3Color;
	float m_fRadius;
};

#define DIRECTIONAL_LIGHT_COUNT 2
#define MAX_POINT_LIGHT_COUNT 1000

class LightingManager
{
public:

	LightingManager(Renderer* renderer, Shader* dirLightShader, Shader* pointLightShader, const unsigned int& nWindowWidth, const unsigned int& nWindowHeight);

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
	Description: Get the point lighting graphics pipeline.
	Return Type const VkPipeline&
	*/
	const VkPipeline& PointLightPipeline();

	/*
	Description: Get the point lighting graphics pipeline layout.
	Return Type const VkPipelineLayout&
	*/
	const VkPipelineLayout& PointLightPipelineLayout();

	/*
	Description: Update the data on a directional light.
	Param:
	    const DirectionalLight& data: The new data for the directional light.
		const unsigned int& nIndex: Index of the directional light to update.
	*/
    void UpdateDirLight(const DirectionalLight& data, const unsigned int& nIndex);

	/*
	Description: Add a new point light to the scene.
	Param:
		const PointLight& data: The data for the new point light.
	*/
	void AddPointLight(const PointLight& data);

	/*
	Description: Update the data on a point light.
	Param:
		const PointLight& data: The new data for the point light.
		const unsigned int& nIndex: Index of the point light to update.
	*/
	void UpdatePointLight(const PointLight& data, const unsigned int& nIndex);

	/*
	Description: Get whether or not the directional lighting was changed since the last update.
	Return Type const bool&
	*/
	const bool& DirLightingChanged();

	/*
	Description: Get whether or not the point lighting was changed since the last update.
	Return Type const bool&
	*/
	const bool& PointLightingChanged();

	/*
	Description: Update directional lighting data on the GPU.
	*/
	inline void UpdateDirLights();

	/*
	Description: Update point lighting data on the GPU.
	*/
	void UpdatePointLights();

	/*
	Description: Re-create lighting graphics pipelines.
	*/
	void RecreatePipelines(Shader* dirLightShader, Shader* pointLightShader, const unsigned int& nWindowWidth, const unsigned int& nWindowHeight);

	/*
	Description: Draw all point lights in the scene.
	Param:
	    VkCommandBuffer& cmdBuffer: The command buffer to embed draw commands in.
	*/
	void DrawPointLights(VkCommandBuffer& cmdBuffer);

private:

	inline void CreatePointLightBuffers();

	inline void CreateDescriptorPool();

	inline void CreateSetLayouts();

	inline void CreateDescriptorSets();

	inline void CreateDirLightingPipeline(Shader* dirLightShader, const unsigned int& nWindowWidth, const unsigned int& nWindowHeight);

	inline void CreatePointLightingPipeline(Shader* pLightShader, const unsigned int& nWindowWidth, const unsigned int& nWindowHeight);

	Renderer* m_renderer;

	// ---------------------------------------------------------------------------------
	// Lights

	Mesh* m_pointLightVolMesh;

	DynamicArray<DirectionalLight> m_dirLights;
	bool m_bDirLightChange;

	DynamicArray<PointLight> m_pointLights;
	unsigned int m_nChangePLightStart; // Starting point of point light changes in the buffer.
	unsigned int m_nChangePLightEnd; // Ending point of the point light changes in the buffer.
	bool m_bPointLightChange;

	// ---------------------------------------------------------------------------------
	// Buffers

	VkBuffer m_dirLightUBO;
	VkDeviceMemory m_dirLightUBOMemory;

	VkBuffer m_pointLightStageInsBuffer;
	VkDeviceMemory m_pointLightStageInsMemory;

	VkBuffer m_pointLightInsBuffer;
	VkDeviceMemory m_pointLightInsMemory;

	// ---------------------------------------------------------------------------------
	// Descriptors
	VkDescriptorPool m_descriptorPool;
	VkDescriptorSetLayout m_dirLightUBOLayout;
	VkDescriptorSet m_dirLightUBOSet;

	// ---------------------------------------------------------------------------------
	// Pipelines

	VkPipeline m_dirLightPipeline;
	VkPipelineLayout m_dirLightPipelineLayout;

	VkPipeline m_pointLightPipeline;
	VkPipelineLayout m_pointLightPipelineLayout;

	// ---------------------------------------------------------------------------------
};

