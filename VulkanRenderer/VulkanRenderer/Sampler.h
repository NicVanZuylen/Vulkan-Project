#pragma once
#include "Table.h"
#include <string>
#include <vulkan/vulkan.h>

class Renderer;

#define DEFAULT_ANISOTROPIC_FILTERING 16.0f

enum EFilterMode 
{
	FILTER_MODE_NEAREST,
	FILTER_MODE_BILINEAR
};

enum ERepeatMode 
{
	REPEAT_MODE_REPEAT,
	REPEAT_MODE_CLAMP_TO_EDGE,
	REPEAT_MODE_CLAMP_TO_EDGE_MIRRORED,
	REPEAT_MODE_DONT_REPEAT
};

class Sampler
{
public:

	Sampler(Renderer* renderer, EFilterMode filterMode = FILTER_MODE_NEAREST, ERepeatMode repeatMode = REPEAT_MODE_REPEAT, float fAnisoTropy = DEFAULT_ANISOTROPIC_FILTERING);

	~Sampler();

	const VkSampler& GetHandle();

private:

	static Table<Sampler> m_samplerTable;

	Renderer* m_renderer;
	std::string m_nameID;
	VkSampler m_handle;
};

