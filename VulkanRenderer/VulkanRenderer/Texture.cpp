#include "Texture.h"
#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

Texture::Texture(Renderer* renderer, const char* szFilePath) 
{
	m_renderer = renderer;
	m_data = nullptr;

	m_nWidth = 0;
	m_nHeight = 0;
	m_nChannels = 0;
	m_bOwnsTexture = true;

	if (!szFilePath)
		return;

	// Load image...
	m_data = stbi_load(szFilePath, &m_nWidth, &m_nHeight, &m_nChannels, STBI_rgb_alpha);

	if(m_data) 
	{
		std::cout << "Successfully loaded image: " << szFilePath << std::endl;

		// Next step.
		StageImage();
	}
	else
	{
		std::cout << "Failed to load image: " << szFilePath << std::endl;
	}
}

Texture::~Texture() 
{
	if (m_bOwnsTexture)
	{
		// Destroy texture image.
		vkDestroyImage(m_renderer->GetDevice(), m_imageHandle, nullptr);
	}
}

void Texture::Bind() 
{
	
}

int Texture::GetWidth() 
{
	return m_nWidth;
}

int Texture::GetHeight() 
{
	return m_nHeight;
}

void Texture::StageImage() 
{
	unsigned long long textureSize = m_nWidth * m_nHeight * sizeof(unsigned int);

	// Create image staging buffer.
	m_renderer->CreateBuffer(textureSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_stagingBuffer, m_stagingMemory);

	// Map memory and copy image to buffer.
	void* buffer = nullptr;
	vkMapMemory(m_renderer->GetDevice(), m_stagingMemory, 0, textureSize, 0, &buffer);

	// Copy memory.
	memcpy_s(buffer, textureSize, m_data, textureSize);

	// Unmap buffer.
	vkUnmapMemory(m_renderer->GetDevice(), m_stagingMemory);

	// Host-side image data is no longer needed.
	stbi_image_free(m_data);
	m_data = nullptr;

	// Next step.
	CreateImageBuffer();
}

void Texture::CreateImageBuffer() 
{
	VkImageCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	createInfo.imageType = VK_IMAGE_TYPE_2D;
	createInfo.extent.width = static_cast<unsigned int>(m_nWidth);
	createInfo.extent.height = static_cast<unsigned int>(m_nHeight);
	createInfo.extent.depth = 1;
	createInfo.mipLevels = 1;
	createInfo.arrayLayers = 1;
	createInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	createInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	createInfo.flags = 0;

	// Create image.
	RENDERER_SAFECALL(vkCreateImage(m_renderer->GetDevice(), &createInfo, nullptr, &m_imageHandle), "Texture Error: Failed to create image object.");

	// Next step.
}