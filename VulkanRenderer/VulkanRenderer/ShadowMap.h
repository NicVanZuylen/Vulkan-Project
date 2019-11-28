#pragma once
#include "RenderModule.h"

class RenderObject;
class Texture;

struct PipelineData;

struct ShadowMapCamera 
{
	glm::mat4 m_viewMat;
	glm::mat4 m_projMat;
};

#define SHADOW_MAPPING_VERT_SHADER_PATH "Shaders/SPIR-V/shadow_map_vert.spv"

class ShadowMap : public RenderModule
{
public:

	ShadowMap(Renderer* renderer, Texture* shadowMap, uint32_t nWidth, uint32_t nHeight, DynamicArray<PipelineData*>* pipelines, VkCommandPool cmdPool, VkRenderPass pass, uint32_t nQueueFamilyIndex);

	~ShadowMap();

	void RecordCommandBuffer(const uint32_t& nPresentImageIndex, const uint32_t& nFrameIndex, const VkFramebuffer& framebuffer, const VkCommandBuffer transferCmdBuf) override;

	void UpdateCamera(glm::vec4 v4LookDirection);

	Texture* GetShadowMapImage();

private:

	// ---------------------------------------------------------------------------------
	// Constructor extensions

	inline void CreateShadowMapCamera();

	inline void CreateDescriptorPool();

	inline void CreateDescriptorSetLayouts();

	inline void CreateDescriptorSets();

	inline void UpdateDescriptorSets();

	inline void CreateRenderPipeline();

	// ---------------------------------------------------------------------------------
	// Template Vulkan structures

	static VkCommandBufferInheritanceInfo m_inheritanceInfo;
	static VkCommandBufferBeginInfo m_beginInfo;

	// ---------------------------------------------------------------------------------
	// Render pipelines

	// Vertex shader used for shadow mapping.
	Shader* m_vertShader;

	VkPipelineLayout m_shadowMapPipelineLayout;
	VkPipeline m_shadowMapPipeline;

	// ---------------------------------------------------------------------------------
	// Shadow map information

	uint32_t m_nShadowMapWidth;
	uint32_t m_nShadowMapHeight;
	Texture* m_shadowMap;

	ShadowMapCamera m_camera;
	uint32_t m_nTransferCamera;

	VkBuffer m_shadowCamStagingBufs[MAX_FRAMES_IN_FLIGHT];
	VkDeviceMemory m_shadowCamStagingMemories[MAX_FRAMES_IN_FLIGHT];

	VkBuffer m_shadowCamUBOs[MAX_FRAMES_IN_FLIGHT];
	VkDeviceMemory m_shadowCamMemories[MAX_FRAMES_IN_FLIGHT];

	// ---------------------------------------------------------------------------------
	// Descriptors

	VkDescriptorPool m_descPool;
	VkDescriptorSetLayout m_camSetLayout;
	VkDescriptorSet m_camDescSets[MAX_FRAMES_IN_FLIGHT];

	// ---------------------------------------------------------------------------------
	// Scene data

	// The pipelines won't actually be used, we only want the renderobjects inside of them.
	DynamicArray<PipelineData*>* m_pipelines;
};

