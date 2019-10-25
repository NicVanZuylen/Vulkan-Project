#pragma once
#include "SubScene.h"
#include "DynamicArray.h"

#define MAX_SUBSCENE_COUNT 5

class Scene
{
public:

	Scene(uint32_t nWindowWidth, uint32_t nWindowHeight, uint32_t nQueueFamilyIndex);

	~Scene();

	void AddSubscene();

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
	// Subscenes

	uint32_t m_nSubSceneCount;
	DynamicArray<SubScene*> m_subScenes;

	// ---------------------------------------------------------------------------------
	// Command buffers

	VkCommandPool m_cmdPool;
	DynamicArray<VkCommandBuffer> m_primaryCmdBuffers;
};

