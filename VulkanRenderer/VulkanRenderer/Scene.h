#pragma once
#include <vulkan/vulkan.h>
#include "glm.hpp"
#include "DynamicArray.h"

class SubScene;
class Renderer;

struct Shader;

// ---------------------------------------------------------------------------------
// Lighting shader directory macros

// Fullscreen quad vertex shader
#define FS_QUAD_SHADER "Shaders/SPIR-V/fs_quad_vert.spv"

// Light volume vertex shader
#define POINT_LIGHT_VERTEX_SHADER "Shaders/SPIR-V/deferred_point_light_vert.spv"

// Lighting shaders
#define DEFERRED_DIR_LIGHT_SHADER "Shaders/SPIR-V/deferred_dir_light_frag.spv"
#define DEFERRED_POINT_LIGHT_SHADER "Shaders/SPIR-V/deferred_point_light_frag.spv"

// ---------------------------------------------------------------------------------

#ifndef MAX_FRAMES_IN_FLIGHT
#define MAX_FRAMES_IN_FLIGHT 2
#endif

class Scene
{
public:

	Scene(Renderer* renderer, uint32_t nWindowWidth, uint32_t nWindowHeight, uint32_t nQueueFamilyIndex);

	~Scene();

	void ResizeOutput(const uint32_t nWidth, const uint32_t nHeight);

	void UpdateCameraView(const glm::mat4& view, const glm::vec4& v4ViewPos);

	void DrawSubscenes(const uint32_t& nPresentImageIndex, const uint64_t nElapsedFrames, const uint32_t& nFrameIndex, VkSemaphore& imageAvailableSemaphore, VkSemaphore& renderFinishedSemaphor, VkFence& frameFence);

	SubScene* GetPrimarySubScene();

	Renderer* GetRenderer();

private:

	// ---------------------------------------------------------------------------------
	// Constructor Extensions

	// Get queue used to submit render & transfer commands.
	inline void GetQueue();

	// Create command pool for transfer commands.
	inline void CreateTransferCmdPool();

	// Create command buffers for transfer commands.
	inline void AllocateTransferCmdBufs();

	// Creates objects used to syncronize rendering with frame presentation etc.
	inline void CreateSyncObjects();

	// ---------------------------------------------------------------------------------
	// Main

	Renderer* m_renderer;
	uint32_t m_nWindowWidth;
	uint32_t m_nWindowHeight;
	uint32_t m_nQueueFamilyIndex;

	VkQueue m_queue;

	// ---------------------------------------------------------------------------------
	// Transfer commands

	VkCommandPool m_transferCmdPool;
	VkCommandBuffer m_transferCmdBufs[MAX_FRAMES_IN_FLIGHT];

	// ---------------------------------------------------------------------------------
	// Shared shaders.

	Shader* m_dirLightShader;
	Shader* m_pointLightShader;

	// ---------------------------------------------------------------------------------
	// Sync Objects

	VkSemaphore m_renderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT]; // Signaled when the frame is complete, frame presentation waits until this is signaled.
	VkSemaphore m_transferCompleteSemaphores[MAX_FRAMES_IN_FLIGHT]; // Signaled when the transfer submission completes execution, rendering waits on this.

	// ---------------------------------------------------------------------------------
	// Subscenes

	uint32_t m_nSubSceneCount;
	SubScene* m_primarySubscene;
};

