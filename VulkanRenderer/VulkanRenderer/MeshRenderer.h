#pragma once
#include <vulkan/vulkan.h>
#include "glm.hpp"
#include "Table.h"
#include "VertexInfo.h"

class Renderer;
class Mesh;
class MeshRenderer;
class Material;

struct PipelineData;
struct Shader;

struct VkCommandBuffer_T;

struct PipelineData
{
	PipelineData();

	Material* m_material;
	VkPipeline m_handle;
	VkPipelineLayout m_layout;
	DynamicArray<EVertexAttribute> m_vertexAttributes;
	DynamicArray<MeshRenderer*> m_renderObjects; // All objects using this pipeline.
};

struct PipelineDataPtr 
{
	PipelineDataPtr();

	PipelineData* m_ptr;
};

struct Instance 
{
	glm::mat4 m_modelMat;
};

class MeshRenderer
{
public:

	static DynamicArray<EVertexAttribute> m_defaultInstanceAttributes;

	/*
	Constructor:
	Param:
	    Renderer* renderer: The renderer this render object associates with.
		Mesh* mesh: The mesh to render.
		Material* material: The material to render with.
		DynamicArray<EVertexAttribute>* instanceAttributes: Array of per-instance vertex attributes for the instance buffer.
		unsigned int nMaxInstanceCount: Maximum amount of instances allowed for this render object.
	*/
	MeshRenderer(Renderer* renderer, Mesh* mesh, Material* material, DynamicArray<EVertexAttribute>* instanceAttributes = &m_defaultInstanceAttributes, unsigned int nMaxInstanceCount = 1);

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

	/*
	Description: Add an instance of this render object.
	Param:
	    Instance& instance: The instance to add.
	*/
	void AddInstance(Instance& instance);

	/*
	Description: Remove an instance of this render object.
	Param:
		const int& nIndex: The index of the instance to remove.
	*/
	void RemoveInstance(const unsigned int& nIndex);

	/*
	Description: Set the values of the instance at the specified index.
	Param:
	    const int& nIndex: The index of the instance to modify.
		Instance& instance: The instance data to use.
	*/
	void SetInstance(const unsigned int& nIndex, Instance& instance);

	/*
	Description: Update instance data on the GPU.
	Param:
	    VkCommandBuffer: The command buffer to record transfer commands to.
	*/
	void UpdateInstanceData(VkCommandBuffer cmdBuffer);

	/*
	Description: Recreate the graphics pipeline this object uses.
	*/
	void RecreatePipeline();

	const Shader* GetShader() const;

	const Material* GetMaterial() const;

private:

	/*
	Description: Create a graphics pipeline or use an existing one for this object.
	Param:
	    DynamicArray<EVertexAttribute*> vertexAttributes: Vertex format for the pipeline & instance buffers.
		bool bRecreate: If true, this will recreate the pipeline, replacing the old one.
	*/
	void CreateGraphicsPipeline(DynamicArray<EVertexAttribute>* vertexAttributes, bool bRecreate = false);

	std::string m_nameID;

	Renderer* m_renderer;
	Material* m_material;
	Mesh* m_mesh;

	// Instance data.
	Instance* m_instanceArray;
	unsigned int m_nInstanceArraySize;
	unsigned int m_nInstanceCount;
	bool m_bInstancesModified;

	VkBuffer m_instanceStagingBuffer;
	VkDeviceMemory m_instanceStagingMemory;

	VkBuffer m_instanceBuffer;
	VkDeviceMemory m_instanceMemory;

	// Pipeline information.
	PipelineData* m_pipelineData;

	static Table<PipelineDataPtr> m_pipelineTable;
	static DynamicArray<PipelineData*> m_allPipelines;
};

