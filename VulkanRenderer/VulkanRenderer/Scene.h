#pragma once
#include "SubScene.h"
#include "DynamicArray.h"

#define MAX_SUBSCENE_COUNT 5

#define FS_QUAD_SHADER "Shaders/SPIR-V/fs_quad_vert.spv"

#define DEFERRED_DIR_LIGHT_SHADER "Shaders/SPIR-V/deferred_dir_light_frag.spv"
#define DEFERRED_POINT_LIGHT_SHADER "Shaders/SPIR-V/deferred_point_light_frag.spv"

class Scene
{
public:

	Scene(uint32_t nWindowWidth, uint32_t nWindowHeight, uint32_t nQueueFamilyIndex);

	~Scene();

	void AddSubscene();

	void DrawSubscenes(const uint32_t& nPresentImageIndex, const uint32_t nFrameIndex, DynamicArray<VkSemaphore>& waitSemaphores, DynamicArray<VkSemaphore>& renderFinishedSemaphores, VkFence& frameFence);

private:

	// ---------------------------------------------------------------------------------
	// Constructor Extensions

	inline void ConstructSubScenes();

	inline void ConstructRenderTargets();

	inline void CreateCommandPool();

	inline void AllocateCmdBuffers();

	// ---------------------------------------------------------------------------------
	// Main

	Renderer* m_renderer;
	uint32_t m_nWindowWidth;
	uint32_t m_nWindowHeight;
	uint32_t m_nGraphicsQueueFamilyIndex;

	// ---------------------------------------------------------------------------------
	// Shared Render Target Images

	Texture* m_colorImage;
	Texture* m_depthImage;
	Texture* m_posImage;
	Texture* m_normalImage;

	// ---------------------------------------------------------------------------------
	// Shared shaders.

	Shader* m_dirLightShader;
	Shader* m_pointLightShader;

	// ---------------------------------------------------------------------------------
	// Subscenes

	uint32_t m_nSubSceneCount;
	SubScene* m_subScene;

	// ---------------------------------------------------------------------------------
	// Command buffers

	VkCommandPool m_cmdPool;
	DynamicArray<VkCommandBuffer> m_primaryCmdBuffers;
};

