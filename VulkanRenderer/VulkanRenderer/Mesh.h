#ifndef MESH_H
#define MESH_H

#include "glm.hpp"
#include "DynamicArray.h"
#include "Renderer.h"
#include <string>

//class Renderer;
class VertexInfo;

struct Vertex
{
	glm::vec4 m_position;
	glm::vec4 m_color;
};

struct ComplexVertex
{
	glm::vec4 m_position;
	glm::vec4 m_normal;
	glm::vec4 m_tangent;
	glm::vec2 m_texCoords;
};

struct MeshCacheData 
{
	uint64_t m_nVertCount;
	uint64_t m_nIndexCount;
	size_t m_nVertOffset;
	size_t m_nIndexOffset;
};

class Mesh 
{
public:

	Mesh(Renderer* renderer, const char* szFilePath);

	Mesh(Renderer* renderer, const char* szFilePath, const VertexInfo* m_format);

	~Mesh();

	/*
	Description: Load the mesh from a file, and any included materials.
	Param:
	    const char* szFilePath: The path to the .obj mesh file.
		unsigned int textureFlags: The texturemaps to load from the obj's materials, by default all maps are loaded.
	*/
	void Load(const char* szFilePath);

	/*
	Description: Bind the VAO of this mesh for use in drawing without instancing.
	Param:
	    VkCommandBuffer& commandBuffer: The command buffer to bind this mesh to.
	*/
	void Bind(VkCommandBuffer& commandBuffer);

	/*
	Description: Bind the VAO of this mesh for use in drawing with instancing.
	Param:
		VkCommandBuffer& commandBuffer: The command buffer to bind this mesh to.
		VkBuffer& instanceBuffer: The instance buffer to bind alongside the vertex buffer.
	*/
	void Bind(VkCommandBuffer& commandBuffer, const VkBuffer& instanceBuffer);

	/*
	Description: Returns the vertex buffer handle of this mesh.
	Return Type: VkBuffer
	*/
	VkBuffer& VertexBuffer();

	/*
	Description: Returns the index buffer handle of this mesh.
	Return Type: VkBuffer
	*/
	VkBuffer& IndexBuffer();

	/*
	Description: Get the amount of vertices in the entire mesh.
	Return Type: unsigned int
	*/
	unsigned int VertexCount();

	/*
	Description: Get the amount of indices in the entire mesh.
	Return Type: unsigned int
	*/
	unsigned int IndexCount();

	/*
	Description: Get the name of this mesh, which should be the name of the file.
	Return Type: std::string&
	*/
	const std::string& GetName();

	/*
	Description: Get the vertex format of this mesh.
	Return Type: const VertexInfo*
	*/
	const VertexInfo* VertexFormat();

	const static VertexInfo defaultFormat;

private:

	/*
	Description: Record the copy command buffer to copy this mesh's staging buffer memory to it's vertex and index buffers.
	*/
	static void RecordCopyCommandBuffer(Renderer* renderer, VkCommandBuffer cmdBuffer, VkBuffer vertStagingBuffer, VkBuffer vertFinalBuffer, VkBuffer indStagingBuffer, VkBuffer indFinalBuffer, unsigned long long vertCopySize, unsigned long long indCopySize);

	/*
	Description: Calculate mesh tangents.
	*/
	void CalculateTangents(DynamicArray<ComplexVertex>& vertices, DynamicArray<unsigned int>& indices);

	/*
	Description: Load and convert mesh from an OBJ file.
	Param:
	    DynamicArray<ComplexVertex>& vertices: The array of output vertices.
		DynamicArray<unsigned int>& indices: The array of output indices.
		const char* path: The file path of the .obj file to load.
	*/
	void LoadOBJ(DynamicArray<ComplexVertex>& vertices, DynamicArray<unsigned int>& indices, const char* path);

	struct Instance
	{
		glm::vec4 m_color;
		float m_modelMat[16];
		float m_normalMat[9];
	};

	// Vulkan handles

	VkBuffer m_vertexBuffer;
	VkDeviceMemory m_vertexMemory;

	VkBuffer m_indexBuffer;
	VkDeviceMemory m_indexMemory;

	// Misc data
	Renderer* m_renderer;

	const char* m_filePath;
	std::string m_name;

	const VertexInfo* m_vertexFormat;

	unsigned int m_totalVertexCount;
	unsigned int m_totalIndexCount;

	bool m_empty;
};

#endif /* MESH_H */