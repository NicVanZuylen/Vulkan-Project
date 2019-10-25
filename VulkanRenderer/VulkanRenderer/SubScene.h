#pragma once
#include <vulkan/vulkan.h>
#include "DynamicArray.h"

class Renderer;
class RenderModule;
class GBufferPass;
class LightingManager;
class Texture;

#define SUB_PASS_COUNT 2
#define POST_SUBPASS_INDEX 2

class SubScene
{
public:

	SubScene(Renderer* renderer, unsigned int nQueueFamilyIndex, unsigned int nOutWidth, unsigned int nOutHeight, bool bPrimary);

	~SubScene();

	void SetImages(Texture* colorImage, Texture* depthImage, Texture* posImage, Texture* normalImage);

	GBufferPass* GetGBufferPass();

	LightingManager* GetLightingManager();

private:

	// ---------------------------------------------------------------------------------
	// Constructor extensions

	inline void CreateAttachmentDescriptions(const DynamicArray<Texture*>& targets, DynamicArray<VkAttachmentDescription>& attachments);

	inline void CreateAttachmentReferences(const uint32_t& nIndexOffset, const DynamicArray<Texture*>& targets, DynamicArray<VkAttachmentReference>& references);

	inline void CreateInputAttachmentReferences(const uint32_t& nIndexOffset, const DynamicArray<Texture*>& targets, DynamicArray<VkAttachmentReference>& references);

	inline void CreateRenderPass();

	// ---------------------------------------------------------------------------------
	// Render pass structures

	static VkAttachmentDescription m_swapChainAttachmentDescription;
	static VkAttachmentDescription m_depthAttachmentDescription;
	static VkAttachmentDescription m_colorAttachmentDescription;
	static VkAttachmentDescription m_vectorAttachmentDescription;

	static VkSubpassDependency m_gBufferDependency; // Subpass dependency for g-buffer subpass.
	static VkSubpassDependency m_lightingDependency; // Subpass dependency for lighting subpass.
	static VkSubpassDependency m_postDependency; // Subpass dependency for all post effects.

	// ---------------------------------------------------------------------------------
	// Main

	Renderer* m_renderer;

	// ---------------------------------------------------------------------------------
	// Modules

	GBufferPass* m_gPass;
	LightingManager* m_lightManager;

	// ---------------------------------------------------------------------------------
	// Render target images.

	Texture* m_colorImage;
	Texture* m_depthImage;
	Texture* m_posImage;
	Texture* m_normalImage;

	DynamicArray<Texture*> m_targetImages;
	DynamicArray<Texture*> m_gBufferImages;

	// ---------------------------------------------------------------------------------
	// Render pass

	VkRenderPass m_pass;

	// ---------------------------------------------------------------------------------
	// Output

	Texture* m_outImage; // Output image of this scene.
};

