#pragma once
#include "Renderer.h"

enum EAttachmentType 
{
	ATTACHMENT_COLOR,
	ATTACHMENT_DEPTH_STENCIL
};

class Texture 
{
public:

	/*
	Constructor: Construct as a texture loaded from an image file.
	Param:
	    Renderer* renderer: The renderer this texture will by use.
		const char* szFilePath: File path of the image to load.
	*/
	Texture(Renderer* renderer, const char* szFilePath);

	/*
	Constructor: Construct as a framebuffer attachment.
	Param:
		Renderer* renderer: The renderer this texture will by use.
		EAttachmentType type: The type of attachment to create.
		unsigned int nWidth: The width of the attachment.
		unsigned int nHeight: The height of the attachment.
		VKFormat format: The image format to use for this attachment.
	*/
	Texture(Renderer* renderer, uint32_t nWidth, uint32_t nHeight, EAttachmentType type = ATTACHMENT_COLOR, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM);

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

	/*
	Description: Get the image format of this texture.
	Return Type: const VkFormat&
	*/
	const VkFormat& Format();

protected:

	/*
	Description: Creates a staging buffer for the texture image.
	*/
	void StageImage();

	/*
	Description: Record image memory barrier command buffer.
	*/
	void RecordImageMemBarrierCmdBuffer(VkCommandBuffer cmdBuffer, VkImageLayout oldLayout, VkImageLayout newLayout, bool bHasStencil = false);

	/*
	Description: Transition the image layout of this texture.
	*/
	void TransitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout, VkFormat format);

	/*
	Description: Record the command buffer to copy image data from the staging buffer to the image buffer.
	*/
	void RecordCopyCommandBuffer(VkCommandBuffer cmdBuffer);

	/*
	Description: Transfer staging buffer contents to the image buffer contents.
	*/
	void TransferContents();

	unsigned char* m_data;
	std::string m_name;
	Renderer* m_renderer;

	VkCommandBuffer m_copyCmdBuffer;

	VkBuffer m_stagingBuffer;
	VkDeviceMemory m_stagingMemory;

	VkFormat m_format;
	VkImage m_imageHandle;
	VkImageView m_imageView;
	VkDeviceMemory m_imageMemory;

	int m_nWidth;
	int m_nHeight;
	int m_nChannels;
	bool m_bOwnsTexture;
};