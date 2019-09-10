#pragma once
#include "Renderer.h"

#ifndef ATTACHMENT_E
#define ATTACHMENT_E

enum EAttachmentType 
{
	ATTACHMENT_COLOR,
	ATTACHMENT_DEPTH_STENCIL
};

#endif

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
		bool bInputAttachment: Whether or not this attachment may be used as a shader stage input.
	*/
	Texture(Renderer* renderer, uint32_t nWidth, uint32_t nHeight, EAttachmentType type = ATTACHMENT_COLOR, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM, bool bInputAttachment = false);

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

	/*
	Description: Get the attachment type of this image.
	Return Type: const EAttachmentType&
	*/
	const EAttachmentType& GetAttachmentType();

	/*
	Description: Get whether or not this attachment has a stencil component.
	Return Type: bool
	*/
	bool HasStencil();

	/*
	Description: Get whether or not this attachment is a presentable image.
	Return Type: bool
	*/
	bool IsPresented();

protected:

	/*
	Description: Creates a staging buffer for the texture image.
	*/
	void StageImage();

	/*
	Description: Record image memory barrier (to transition image layout) command buffer.
	Param:
	    VkCommandBuffer cmdBuffer: The command buffer to record to.
		VkImageLayout: oldLayout: The image layout to transition from.
		VkImageLayout: newLayout: The target image layout to transition to.
		bool bHasStencil: Whether or not the image has a stencil component (if it is a depth/stencil image).
	*/
	void RecordImageMemBarrierCmdBuffer(VkCommandBuffer cmdBuffer, VkImageLayout oldLayout, VkImageLayout newLayout, bool bHasStencil = false);

	/*
	Description: Create a VkImage object with the provided properties.
	Param:
	    VkImage& image: VkImage handle reference.
		VkDeviceMemory& imageMemory: Image memory buffer handle reference.
		const unsigned int& nWidth: The width of the image viewport.
		const unsigned int& nHeight: The height of the image viewport.
		VkFormat format: The format of the image.
		VkImageTiling tiling: The tiling properties of the image.
		VKImageUsageFlags usage: Flags detailing how the image will be used.
	*/
	void CreateImage(VkImage& image, VkDeviceMemory& imageMemory, const uint32_t& nWidth, const uint32_t& nHeight, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage);

	/*
	Description: Create an image view for the provided image, using the specified format and aspect flags.
	Param:
		const VkImage& image: VkImage handle reference.
		VkImageView& view: The handle for the new image view.
		VkFormat format: The format of the image.
		VKImageAspectFlags aspectFlags: Flags to specify if this a color image, depth image, etc.
	*/
	void CreateImageView(const VkImage& image, VkImageView& view, VkFormat format, VkImageAspectFlags aspectFlags);

	/*
	Description: Transition the image layout of this texture.
	Param:
	    VkImageLayout oldLayout: The current layout to be transitioned from.
		VkImageLayout newLayout: The target layout to transition to.
		VkFormat format: The format of the image.
	*/
	void TransitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout, VkFormat format);

	/*
	Description: Record the command buffer to copy image data from the staging buffer to the image buffer.
	Param:
	    VkCommandBuffer cmdBuffer: The command buffer handle to record to.
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

	EAttachmentType m_type;
	VkFormat m_format;
	VkImage m_imageHandle;
	VkImageView m_imageView;
	VkDeviceMemory m_imageMemory;

	int m_nWidth;
	int m_nHeight;
	int m_nChannels;
	bool m_bPresented;
	bool m_bHasStencil;
	bool m_bOwnsTexture;
};