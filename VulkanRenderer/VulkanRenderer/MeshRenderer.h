#pragma once
#include <vulkan/vulkan.h>
#include "glm.hpp"
#include "Table.h"

class Renderer;
class Mesh;
class MeshRenderer;

struct PipelineData;
struct Shader;

struct VkCommandBuffer_T;

struct PipelineData
{
	PipelineData();

	DynamicArray<MeshRenderer*> m_renderObjects; // All objects using this pipeline.
	VkPipeline m_handle;
	VkPipelineLayout m_layout;
};

struct PipelineDataPtr 
{
	PipelineDataPtr();

	PipelineData* m_ptr;
};

struct VertexType 
{
	/*
	Description: Get the binding description for this vertex structure.
	*/
	virtual VkVertexInputBindingDescription BindingDescription();

	/*
	Description: Get the vertex attribues for this vertex structure.
	*/
    virtual void AttributeDescriptions(DynamicArray<VkVertexInputAttributeDescription>& outDescriptions);
};

//struct Vertex : public VertexType

class MeshRenderer
{
public:

	MeshRenderer(Renderer* renderer, Mesh* mesh, Shader* shader);

	~MeshRenderer();

	/*
	Description: Get all existing RenderObject pipelines.
	Return Type: DynArr<PipelineData*>& 
	*/
	static DynamicArray<PipelineData*>& Pipelines();

	/*
	Description: Add the draw commands of this object to the externally recorded command buffer.
	Param:
	    VkCommandBuffer& cmdBuffer: The command buffer to record to.
	*/
	void CommandDraw(VkCommandBuffer_T* cmdBuffer);

	const Shader* GetShader();

private:

	// Create the graphics pipeline for this render object.
	void CreateGraphicsPipeline();

	std::string m_nameID;

	Renderer* m_renderer;
	Shader* m_shader;
	Mesh* m_mesh;

	// Pipeline information.
	PipelineData* m_pipelineData;

	static Table<PipelineDataPtr> m_pipelineTable;
	static DynamicArray<PipelineData*> m_allPipelines;
};

