#pragma once
#include "Renderer.h"

class Texture 
{
public:

	Texture(Renderer* renderer, const char* szFilePath);

	~Texture();

	/*
	Description: Bind this texture to a GPU texture unit.
	*/
	void Bind();

	/*
	Description: Get the width in pixels of the texture.
	Return Type: int
	*/
	int GetWidth();

	/*
	Description: Get the height in pixels of the texture.
	Return Type: int
	*/
	int GetHeight();

protected:

	/*
	Description: Creates a staging buffer for the texture image.
	*/
	void StageImage();

	/*
	Description: Create the device-side image used for sampling in fragment shaders.
	*/
	void CreateImageBuffer();

	unsigned char* m_data;
	Renderer* m_renderer;

	VkBuffer m_stagingBuffer;
	VkDeviceMemory m_stagingMemory;

	VkImage m_imageHandle;
	VkDeviceMemory m_imageMemory;

	int m_nWidth;
	int m_nHeight;
	int m_nChannels;
	bool m_bOwnsTexture;
};