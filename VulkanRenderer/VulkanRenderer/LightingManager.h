#pragma once
#include <vulkan/vulkan.h>
#include "glm.hpp"
#include "DynamicArray.h"
#include "RenderModule.h"
#include "Texture.h"

class Renderer;
class Mesh;

struct Shader;

struct GlobalDirLightData 
{
	int m_nCount;
	int m_nPadding[3];
};

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

#define MAX_DIRECTIONAL_LIGHTS 4
#define MAX_POINT_LIGHT_COUNT 1000

class LightingManager : public RenderModule
{
public:

	LightingManager(Renderer* renderer, Shader* dirLightShader, Shader* pointLightShader, VkDescriptorSet* mvpUBOSets, VkDescriptorSet gBufferInputSet,
		const unsigned int& nWindowWidth, const unsigned int& nWindowHeight, 
		VkCommandPool cmdPool, VkRenderPass pass, VkDescriptorSetLayout uboLayout, VkDescriptorSetLayout gBufferLayout, unsigned int nQueueFamilyIndex);

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
	Description: Add a directional light to the scene.
	Param:
		DirectionalLight data: The data for the new directional light.
	*/
	void AddDirLight(DirectionalLight data);

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
		PointLight data: The data for the new point light.
	*/
	void AddPointLight(PointLight data);

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
	Param:
	    VkCommandBuffer cmdBuffer: The transfer command buffer to record to.
	*/
	void UpdatePointLights(VkCommandBuffer cmdBuffer);

	/*
	Description: Run when the subscene output resolution is modified.
	*/
	void OnOutputResize(const RenderModuleResizeData& resizeData) override;

	/*
	Description: Re-create lighting graphics pipelines.
	*/
	void RecreatePipelines(Shader* dirLightShader, Shader* pointLightShader, const unsigned int& nWindowWidth, const unsigned int& nWindowHeight);

	/*
	Description: Record lighting pass command buffer.
	*/
	void RecordCommandBuffer(const uint32_t& nPresentImageIndex, const uint32_t& nFrameIndex, const VkFramebuffer& framebuffer, const VkCommandBuffer transferCmdBuf) override;

private:

	inline void CreatePointLightBuffers();

	inline void CreateDescriptorPool();

	inline void CreateSetLayouts();

	inline void CreateDescriptorSets();

	inline void CreateDirLightingPipeline(const unsigned int& nWindowWidth, const unsigned int& nWindowHeight, bool bCreateLayout = true);

	inline void CreatePointLightingPipeline(const unsigned int& nWindowWidth, const unsigned int& nWindowHeight, bool bCreateLayout = true);

	// ---------------------------------------------------------------------------------
	// Template Vulkan Structures

	static VkCommandBufferInheritanceInfo m_inheritanceInfo;
	static VkCommandBufferBeginInfo m_beginInfo;

	// ---------------------------------------------------------------------------------
	// Lights

	Mesh* m_pointLightVolMesh;

	Shader* m_dirLightShader;
	Shader* m_pointLightShader;

	DynamicArray<DirectionalLight> m_dirLights;
	GlobalDirLightData m_globalDirData;
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

	VkDescriptorSet m_mvpUBOSets[MAX_FRAMES_IN_FLIGHT];
	VkDescriptorSet m_gBufferInputSet;

	VkDescriptorSetLayout m_mvpUBOSetLayout;
	VkDescriptorSetLayout m_gBufferSetLayout;

	// ---------------------------------------------------------------------------------
	// Pipelines

	VkPipeline m_dirLightPipeline;
	VkPipelineLayout m_dirLightPipelineLayout;

	VkPipeline m_pointLightPipeline;
	VkPipelineLayout m_pointLightPipelineLayout;

	// ---------------------------------------------------------------------------------
};

