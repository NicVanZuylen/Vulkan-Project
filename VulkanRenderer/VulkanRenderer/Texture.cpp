#include "Texture.h"
#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

Texture::Texture(Renderer* renderer, const char* szFilePath) 
{
	m_renderer = renderer;
	m_name = szFilePath;
	m_name = m_name.substr(m_name.find_last_of("/") + 1); // Remove the rest of the path from the name, to reduce memory usage and hashing time.
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

		// Stage the image, create the final image buffer and transfer contents, image layouts and access flags.
		StageImage();

		// Destroy staging buffer.
		vkDestroyBuffer(renderer->GetDevice(), m_stagingBuffer, nullptr);
		vkFreeMemory(renderer->GetDevice(), m_stagingMemory, nullptr);
	}
	else
	{
		std::cout << "Failed to load image: " << szFilePath << std::endl;
	}
}

Texture::Texture(Renderer* renderer, uint32_t nWidth, uint32_t nHeight, EAttachmentType type, VkFormat format)
{
	m_renderer = renderer;
	m_nWidth = nWidth;
	m_nHeight = nHeight;
	m_bOwnsTexture = true;

	m_format = format;

	VkImageAspectFlags aspect;

	switch (type)
	{
	case ATTACHMENT_COLOR:
		m_nChannels = 4;
		m_name = "COLOR_ATTACHMENT-" + std::to_string(m_nChannels) + "ch\n";

		aspect = VK_IMAGE_ASPECT_COLOR_BIT;

	    m_renderer->CreateImage(m_imageHandle, m_imageMemory, m_nWidth, m_nHeight, m_format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
		m_renderer->CreateImageView(m_imageHandle, m_imageView, m_format, aspect);

		// Transition layout.
		TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, m_format);

		break;

	case ATTACHMENT_DEPTH_STENCIL:
		m_nChannels = 1;
		m_name = "DEPTH_STENCIL_ATTACHMENT-" + std::to_string(m_nChannels) + "ch\n";

		m_renderer->CreateImage(m_imageHandle, m_imageMemory, m_nWidth, m_nHeight, m_format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

		aspect = VK_IMAGE_ASPECT_DEPTH_BIT;

		// Include stencil aspect if the format supports it.
		if (format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT)
			aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;

		m_renderer->CreateImageView(m_imageHandle, m_imageView, m_format, aspect);

		// Transition layout.
		TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, m_format);

		break;

	default:
		break;
	}
}

Texture::~Texture() 
{
	if (m_bOwnsTexture)
	{
		m_renderer->WaitGraphicsIdle();

		// Destroy texture image.
		vkDestroyImageView(m_renderer->GetDevice(), m_imageView, nullptr);
		vkDestroyImage(m_renderer->GetDevice(), m_imageHandle, nullptr);
		vkFreeMemory(m_renderer->GetDevice(), m_imageMemory, nullptr);
	}
}

const std::string& Texture::GetName() const
{
	return m_name;
}

int Texture::GetWidth() const
{
	return m_nWidth;
}

int Texture::GetHeight() const
{
	return m_nHeight;
}

const VkImageView& Texture::ImageView() const
{
	return m_imageView;
}

const VkFormat& Texture::Format() 
{
	return m_format;
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

	m_format = VK_FORMAT_R8G8B8A8_UNORM;

	// Create image.
	m_renderer->CreateImage(m_imageHandle, m_imageMemory, m_nWidth, m_nHeight, m_format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

	// Next step.
	TransferContents();
}

void Texture::TransitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout, VkFormat format) 
{
	Renderer::TempCmdBuffer tmpCmdBuffer = m_renderer->CreateTempCommandBuffer();

	bool bStencilFormat = format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
	RecordImageMemBarrierCmdBuffer(tmpCmdBuffer.m_handle, oldLayout, newLayout, bStencilFormat);

	m_renderer->UseAndDestroyTempCommandBuffer(tmpCmdBuffer);
}

void Texture::RecordImageMemBarrierCmdBuffer(VkCommandBuffer cmdBuffer, VkImageLayout oldLayout, VkImageLayout newLayout, bool bHasStencil)
{
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	beginInfo.pInheritanceInfo = nullptr;
	beginInfo.pNext = nullptr;

	// Begin recording.
	RENDERER_SAFECALL(vkBeginCommandBuffer(cmdBuffer, &beginInfo), "Texture Error: Failed to begin recording of copy command buffer.");

	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destStage;

	VkImageMemoryBarrier memBarrier = {};
	memBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	memBarrier.oldLayout = oldLayout;
	memBarrier.newLayout = newLayout;
	memBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	memBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	memBarrier.image = m_imageHandle;
	memBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	memBarrier.subresourceRange.baseMipLevel = 0;
	memBarrier.subresourceRange.levelCount = 1;
	memBarrier.subresourceRange.baseArrayLayer = 0;
	memBarrier.subresourceRange.layerCount = 1;

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) 
	{
		memBarrier.srcAccessMask = 0;
		memBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) 
	{
		memBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT; // Change aspect mask to depth.
		memBarrier.srcAccessMask = 0;
		memBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		// Add stencil aspect if the format supports it.
		if (bHasStencil)
			memBarrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		memBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		memBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else 
		std::cout << "Texture Warning: Attempting unsupported layout transition!" << std::endl;

	// Change image layout.
	vkCmdPipelineBarrier(cmdBuffer, sourceStage, destStage, 0, 0, nullptr, 0, nullptr, 1, &memBarrier);

	// End recording.
	RENDERER_SAFECALL(vkEndCommandBuffer(cmdBuffer), "Texture Error: Failed to end copy command buffer recording.");
}

void Texture::RecordCopyCommandBuffer(VkCommandBuffer cmdBuffer)
{
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	beginInfo.pInheritanceInfo = nullptr;
	beginInfo.pNext = nullptr;

	// Begin recording.
	RENDERER_SAFECALL(vkBeginCommandBuffer(cmdBuffer, &beginInfo), "Texture Error: Failed to begin recording of copy command buffer.");

	unsigned long long textureSize = m_nWidth * m_nHeight * sizeof(unsigned int);

	VkBufferImageCopy copyRegion = {};
	copyRegion.bufferRowLength = 0;
	copyRegion.bufferImageHeight = 0;
	copyRegion.bufferOffset = 0;
	copyRegion.imageExtent = { static_cast<unsigned int>(m_nWidth), static_cast<unsigned int>(m_nHeight), 1 };
	copyRegion.imageOffset = { 0, 0, 0 };
	copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copyRegion.imageSubresource.mipLevel = 0;
	copyRegion.imageSubresource.baseArrayLayer = 0;
	copyRegion.imageSubresource.layerCount = 1;

	// Copy vertex staging buffer contents to vertex final buffer contents.
	vkCmdCopyBufferToImage(cmdBuffer, m_stagingBuffer, m_imageHandle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

	// End recording.
	RENDERER_SAFECALL(vkEndCommandBuffer(cmdBuffer), "Texture Error: Failed to end copy command buffer recording.");
}

void Texture::TransferContents() 
{
	// Transition layout to transfer destination optimal layout.
	TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, m_format);

	// Copy staging buffer to image.
	Renderer::TempCmdBuffer tmpCmdBuffer = m_renderer->CreateTempCommandBuffer();
	RecordCopyCommandBuffer(tmpCmdBuffer.m_handle);
	m_renderer->UseAndDestroyTempCommandBuffer(tmpCmdBuffer);

	// Transition image layout to shader read only optimal layout.
	TransitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_format);

	// Create image view.
	m_renderer->CreateImageView(m_imageHandle, m_imageView, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
}