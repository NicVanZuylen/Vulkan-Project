#ifndef MESH_H
#define MESH_H

#include "glm.hpp"
#include "DynamicArray.h"
#include <string>
#include <vulkan/vulkan.h>

class Renderer;
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

class Mesh 
{
public:

	Mesh(Renderer* renderer, const char* szFilePath);

	Mesh(Renderer* renderer, const char* szFilePath, VertexInfo* m_format);

	~Mesh();

	/*
	Description: Load the mesh from a file, and any included materials.
	Param:
	    const char* szFilePath: The path to the .obj mesh file.
		unsigned int textureFlags: The texturemaps to load from the obj's materials, by default all maps are loaded.
	*/
	void Load(const char* szFilePath);

	/*
	Description: Bind the VAO of this mesh for use in drawing.
	Param:
	    VkCommandBuffer& commandBuffer: The command buffer to bind this mesh to.
	*/
	void Bind(VkCommandBuffer& commandBuffer);

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

	static VertexInfo defaultFormat;

private:

	/*
	Description: Create the copy command buffer used for copying memory from the staging buffers to vertex and index buffer memory.
	*/
	static void CreateCopyCommandBuffer(Renderer* renderer);

	/*
	Description: Record the copy command buffer to copy this mesh's staging buffer memory to it's vertex and index buffers.
	*/
	static void RecordCopyCommandBuffer(Renderer* renderer, VkBuffer vertStagingBuffer, VkBuffer vertFinalBuffer, VkBuffer indStagingBuffer, VkBuffer indFinalBuffer, unsigned long long vertCopySize, unsigned long long indCopySize);

	/*
	Description: Calculate mesh tangents.
	*/
	void CalculateTangents(DynamicArray<ComplexVertex>& vertices, DynamicArray<unsigned int>& indices);

	struct Instance
	{
		glm::vec4 m_color;
		float m_modelMat[16];
		float m_normalMat[9];
	};

	// Vulkan handles
	static VkCommandBuffer m_copyCmdBuffer;

	VkBuffer m_vertexBuffer;
	VkDeviceMemory m_vertexMemory;

	VkBuffer m_indexBuffer;
	VkDeviceMemory m_indexMemory;

	// Misc data
	Renderer* m_renderer;

	const char* m_filePath;
	std::string m_name;

	VertexInfo* m_vertexFormat;

	unsigned int m_totalVertexCount;
	unsigned int m_totalIndexCount;

	bool m_empty;
};

#endif /* MESH_H */