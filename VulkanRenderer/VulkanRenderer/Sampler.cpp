#include "Sampler.h"
#include "Renderer.h"
#include <sstream>

Table<Sampler> Sampler::m_samplerTable;

Sampler::Sampler(Renderer* renderer, EFilterMode filterMode = FILTER_MODE_NEAREST, ERepeatMode repeatMode = REPEAT_MODE_REPEAT, float fAnisoTropy)
{
	m_renderer = renderer;
	m_handle = nullptr;
	m_nameID = "";

	VkSamplerCreateInfo sampCreateInfo = {};
	sampCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

	// Sampler repeat mode.
	switch (repeatMode)
	{
	case REPEAT_MODE_REPEAT:

		sampCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sampCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;

		m_nameID += "REPEAT|";
		break;

	case REPEAT_MODE_CLAMP_TO_EDGE:

		sampCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

		m_nameID += "CLAMP_EDGE|";
		break;

	case REPEAT_MODE_CLAMP_TO_EDGE_MIRRORED:

		sampCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
		sampCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;

		m_nameID += "CLAMP_EDGE_MIRROR|";
		break;

	case REPEAT_MODE_DONT_REPEAT:

		sampCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		sampCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;

		m_nameID += "DONT_REPEAT|";
		break;
	}

	sampCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

	// Texture filtering.
	switch (filterMode)
	{
	case FILTER_MODE_NEAREST:

		sampCreateInfo.minFilter = VK_FILTER_NEAREST;
		sampCreateInfo.magFilter = VK_FILTER_NEAREST;

		m_nameID += "NEAREST|";
		break;

	case FILTER_MODE_BILINEAR:

		sampCreateInfo.minFilter = VK_FILTER_LINEAR;
		sampCreateInfo.magFilter = VK_FILTER_LINEAR;

		m_nameID += "BILINEAR|";
		break;
	}

	// Anisotropic filtering.
	sampCreateInfo.anisotropyEnable = fAnisoTropy > 0.0f;
	sampCreateInfo.maxAnisotropy = fAnisoTropy;

	std::ostringstream valStr;
	valStr.precision(1);
	valStr << std::fixed << fAnisoTropy;

	m_nameID += "A:" + valStr.str();

	sampCreateInfo.borderColor = VkBorderColor::VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	sampCreateInfo.compareEnable = VK_FALSE;
	sampCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	sampCreateInfo.unnormalizedCoordinates = VK_FALSE;
	sampCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampCreateInfo.mipLodBias = 0.0f;
	sampCreateInfo.minLod = 0.0f;
	sampCreateInfo.maxLod = 0.0f;

	RENDERER_SAFECALL(vkCreateSampler(m_renderer->GetDevice(), &sampCreateInfo, nullptr, &m_handle), "Texture Error: Failed to create image sampler.");
}


Sampler::~Sampler()
{
	vkDestroySampler(m_renderer->GetDevice(), m_handle, nullptr);
	m_handle = nullptr;
}

const VkSampler& Sampler::GetHandle() const
{
	return m_handle;
}

const std::string& Sampler::GetNameID() const 
{
	return m_nameID;
}