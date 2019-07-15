#pragma once
#include "Renderer.h"

class Texture 
{
public:

	Texture(Renderer* renderer, const char* szFilePath);

	~Texture();

	/*
	Description: Get the name of the texture file.
	*/
	const std::string& GetName() const;

	/*
	Description: Get the width in pixels of the texture.
	Return Type: int
	*/
	int GetWidth() const;

	/*
	Description: Get the height in pixels of the texture.
	Return Type: int
	*/
	int GetHeight() const;

	/*
	Description: Get Vulkan image view handle for this texture.
	Return Type: const VkImageView&
	*/
	const VkImageView& ImageView() const;

protected:

	/*
	Description: Creates a staging buffer for the texture image.
	*/
	void StageImage();

	/*
	Description: Create the device-side image used for sampling in fragment shaders.
	*/
	void CreateImageBuffer();

	/*
	Description: Record image memory barrier command buffer.
	*/
	void RecordImageMemBarrierCmdBuffer(VkCommandBuffer cmdBuffer, VkImageLayout oldLayout, VkImageLayout newLayout);

	/*
	Description: Transition the image layout of this texture.
	*/
	void TransitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout);

	/*
	Description: Record the command buffer to copy image data from the staging buffer to the image buffer.
	*/
	void RecordCopyCommandBuffer(VkCommandBuffer cmdBuffer);

	/*
	Description: Transfer staging buffer contents to the image buffer contents.
	*/
	void TransferContents();

	/*
	Description: Create the image view object used by samplers in shaders.
	*/
	void CreateTextureImageView();

	unsigned char* m_data;
	std::string m_name;
	Renderer* m_renderer;

	VkCommandBuffer m_copyCmdBuffer;

	VkBuffer m_stagingBuffer;
	VkDeviceMemory m_stagingMemory;

	VkImage m_imageHandle;
	VkImageView m_imageView;
	VkDeviceMemory m_imageMemory;

	int m_nWidth;
	int m_nHeight;
	int m_nChannels;
	bool m_bOwnsTexture;
};