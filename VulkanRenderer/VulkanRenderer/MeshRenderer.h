#pragma once

class Renderer;

struct PipelineInfo;
struct Shader;

struct VkCommandBuffer_T;

class MeshRenderer
{
public:

	MeshRenderer(const Shader* shader, Renderer* renderer);

	~MeshRenderer();

	/*
	Description: Add the draw commands of this object to the externally recorded command buffer.
	Param:
	    VkCommandBuffer& cmdBuffer: The command buffer to record to.
	*/
	void CommandDraw(VkCommandBuffer_T* cmdBuffer);

	const Shader* GetShader();

	VkCommandBuffer_T* GetDrawCommands(const unsigned int& frameBufferIndex);

private:

	// Creates the graphics pipeline for this mesh.
	void CreateCommandBuffers();

	// Defines the actions executed when this object is drawn.
	void RecordCommandBuffers();

	Renderer* m_renderer;
	const Shader* m_shader;
	// Mesh pointer would go here.

	VkCommandBuffer_T** m_commandBuffers;
	int m_commandBufferCount;
};

