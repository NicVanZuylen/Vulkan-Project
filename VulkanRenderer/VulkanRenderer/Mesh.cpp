#include "Mesh.h"
#include "VertexInfo.h"
#include "RenderObject.h"
#include "Renderer.h"
#include <vector>
#include <iostream>

// Using tiny obj loader header lib for .obj file loading.
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

const VertexInfo Mesh::defaultFormat = VertexInfo({ VERTEX_ATTRIB_FLOAT4, VERTEX_ATTRIB_FLOAT4, VERTEX_ATTRIB_FLOAT4, VERTEX_ATTRIB_FLOAT2 });

Mesh::Mesh(Renderer* renderer, const char* filePath) 
{
	m_renderer = renderer;
	m_empty = true;
	m_filePath = filePath;
	
	m_vertexFormat = &defaultFormat;

	// Use the filename as the name.
	std::string tmpName = filePath;
	m_name = "|" + tmpName.substr(tmpName.find_last_of('/') + 1) + "|";

	Load(filePath);

	m_empty = false;
}

Mesh::Mesh(Renderer* renderer, const char* filePath, const VertexInfo* vertexFormat)
{
	m_renderer = renderer;
	m_empty = true;
	m_filePath = filePath;
	m_vertexFormat = vertexFormat;

	// Use the filename as the name.
	std::string tmpName = filePath;
	m_name = "|" + tmpName.substr(tmpName.find_last_of('/') + 1) + "|";

	Load(filePath);

	m_empty = false;
}

Mesh::~Mesh() 
{
	if(!m_empty) 
	{
		m_renderer->WaitGraphicsIdle();

		vkFreeMemory(m_renderer->GetDevice(), m_vertexMemory, nullptr);
		vkFreeMemory(m_renderer->GetDevice(), m_indexMemory, nullptr);

		vkDestroyBuffer(m_renderer->GetDevice(), m_vertexBuffer, nullptr);
		vkDestroyBuffer(m_renderer->GetDevice(), m_indexBuffer, nullptr);
	}
}

void Mesh::Load(const char* filePath) 
{
	// Delete old mesh if there is one.
	if(!m_empty) 
	{
		m_renderer->WaitGraphicsIdle();

		vkFreeMemory(m_renderer->GetDevice(), m_vertexMemory, nullptr);
		vkFreeMemory(m_renderer->GetDevice(), m_indexMemory, nullptr);

		vkDestroyBuffer(m_renderer->GetDevice(), m_vertexBuffer, nullptr);
		vkDestroyBuffer(m_renderer->GetDevice(), m_indexBuffer, nullptr);
	}

	m_filePath = filePath;

	// -----------------------------------------------------------------------------------------
	// Mesh loading from obj

	// Array of all vertices of all mesh chunks, for a single mesh VAO.
	DynamicArray<ComplexVertex> wholeMeshVertices;
	DynamicArray<unsigned int> wholeMeshIndices;

	// -----------------------------------------------------------------------------------------
	// Cache reading

	std::string cachePath = m_filePath;
	size_t nExtensionStart = cachePath.find_last_of(".");

	cachePath.erase(nExtensionStart); // Remove old file extension.
	cachePath.append(".mcache"); // Append new file extension.

	std::ifstream cacheInStream(cachePath.c_str(), std::ios::binary | std::ios::in);

	unsigned long long vertBufSize = 0;
	unsigned long long indexBufSize = 0;

	// Read cache if one is found.
	if(cacheInStream.good()) 
	{
		std::cout << "Mesh: Reading cache file at: " << cachePath << "\n";

		MeshCacheData inCacheData;

		// Read cache data...
		cacheInStream.read((char*)&inCacheData, sizeof(MeshCacheData));

		// Resize vertex array.
		wholeMeshVertices.SetSize(static_cast<int>(inCacheData.m_nVertCount));
		wholeMeshVertices.SetCount(static_cast<int>(inCacheData.m_nVertCount));

		// Move read offset to vertex start offset.
		cacheInStream.seekg(inCacheData.m_nVertOffset);

		// Read vertex data...
		cacheInStream.read((char*)wholeMeshVertices.Data(), inCacheData.m_nVertCount * sizeof(ComplexVertex));

		// Resize index array.
		wholeMeshIndices.SetSize(static_cast<int>(inCacheData.m_nIndexCount));
		wholeMeshIndices.SetCount(static_cast<int>(inCacheData.m_nIndexCount));

		// Move read offset to index start offset.
		cacheInStream.seekg(inCacheData.m_nIndexOffset);

		// Read index data...
		cacheInStream.read((char*)wholeMeshIndices.Data(), inCacheData.m_nIndexCount * sizeof(unsigned int));

		// Set buffer sizes.
		vertBufSize = sizeof(ComplexVertex) * wholeMeshVertices.Count();
		indexBufSize = sizeof(unsigned int) * wholeMeshIndices.Count();

		// Release file handle.
		cacheInStream.close();
	}
	else // Otherwise load and convert OBJ file.
	{
		LoadOBJ(wholeMeshVertices, wholeMeshIndices, m_filePath);

		vertBufSize = sizeof(ComplexVertex) * wholeMeshVertices.Count();
		indexBufSize = sizeof(unsigned int) * wholeMeshIndices.Count();

		// -----------------------------------------------------------------------------------------
		// Cache writing

		std::ofstream cacheOutStream(cachePath.c_str(), std::ios::binary | std::ios::out);

		MeshCacheData outCacheData;
		outCacheData.m_nVertCount = wholeMeshVertices.Count();
		outCacheData.m_nIndexCount = wholeMeshIndices.Count();
		outCacheData.m_nVertOffset = sizeof(MeshCacheData);
		outCacheData.m_nIndexOffset = outCacheData.m_nVertOffset + vertBufSize;

		if (cacheOutStream.good())
		{
			std::cout << "Mesh: Writing cache file at: " << cachePath << "\n";

			// Write cache data.
			cacheOutStream.write((const char*)&outCacheData, sizeof(MeshCacheData));

			// Seek to vertex data start offset.
			cacheOutStream.seekp(outCacheData.m_nVertOffset);

			// Write vertex data...
			cacheOutStream.write((const char*)wholeMeshVertices.Data(), vertBufSize);

			// Seek to index data start offset.
			cacheOutStream.seekp(outCacheData.m_nIndexOffset);

			// Write index data.
			cacheOutStream.write((const char*)wholeMeshIndices.Data(), indexBufSize);

			// Release file handle.
			cacheOutStream.close();
		}
	}

	// -----------------------------------------------------------------------------------------
	// Buffers

	// Create new vertex staging buffer.
	VkBuffer vertexStagingBuffer;
	VkDeviceMemory vertStagingBufferMemory;
	m_renderer->CreateBuffer(vertBufSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vertexStagingBuffer, vertStagingBufferMemory);

	VkBuffer indexStagingBuffer;
	VkDeviceMemory indexStagingBufMemory;
	m_renderer->CreateBuffer(indexBufSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, indexStagingBuffer, indexStagingBufMemory);

	// Create vertex buffer.
	m_renderer->CreateBuffer(vertBufSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_vertexBuffer, m_vertexMemory);

	// Create index buffer.
	m_renderer->CreateBuffer(indexBufSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_indexBuffer, m_indexMemory);

	// Copy vertices to the vertex staging buffer.
	void* bufMemory = nullptr;
	RENDERER_SAFECALL(vkMapMemory(m_renderer->GetDevice(), vertStagingBufferMemory, 0, vertBufSize, 0, &bufMemory), "Renderer Error: Failed to map staging buffer memory.");

	memcpy_s(bufMemory, vertBufSize, wholeMeshVertices.Data(), vertBufSize);

	vkUnmapMemory(m_renderer->GetDevice(), vertStagingBufferMemory);

	bufMemory = nullptr;

	// Copy indices to the index staging buffer.
	RENDERER_SAFECALL(vkMapMemory(m_renderer->GetDevice(), indexStagingBufMemory, 0, indexBufSize, 0, &bufMemory), "Renderer Error: Failed to map staging buffer memory.");

	memcpy_s(bufMemory, indexBufSize, wholeMeshIndices.Data(), indexBufSize);

	vkUnmapMemory(m_renderer->GetDevice(), indexStagingBufMemory);

	// Copy staging buffer contents to vertex buffer contents.
	Renderer::TempCmdBuffer tempCopyCmdBuffer = m_renderer->CreateTempCommandBuffer();
	RecordCopyCommandBuffer(m_renderer, tempCopyCmdBuffer.m_handle, vertexStagingBuffer, m_vertexBuffer, indexStagingBuffer, m_indexBuffer, vertBufSize, indexBufSize);
	m_renderer->UseAndDestroyTempCommandBuffer(tempCopyCmdBuffer);

	// Destroy staging buffers.
	vkFreeMemory(m_renderer->GetDevice(), vertStagingBufferMemory, nullptr);
	vkDestroyBuffer(m_renderer->GetDevice(), vertexStagingBuffer, nullptr);

	vkFreeMemory(m_renderer->GetDevice(), indexStagingBufMemory, nullptr);
	vkDestroyBuffer(m_renderer->GetDevice(), indexStagingBuffer, nullptr);

	m_totalVertexCount = static_cast<unsigned int>(wholeMeshVertices.GetSize());
	m_totalIndexCount = static_cast<unsigned int>(wholeMeshIndices.GetSize());

	// -----------------------------------------------------------------------------------------
}

void Mesh::Bind(VkCommandBuffer& commandBuffer)
{
	VkBuffer vertBuffers[] = { m_vertexBuffer };
	size_t offsets[] = { 0 };

	// Bind vertex buffers.
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertBuffers, offsets);

	// Bind index buffer.
	vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);
}

void Mesh::Bind(VkCommandBuffer& commandBuffer, const VkBuffer& instanceBuffer) 
{
	VkBuffer vertBuffers[] = { m_vertexBuffer, instanceBuffer };
	size_t offsets[] = { 0, 0 };

	// Bind vertex buffers.
	vkCmdBindVertexBuffers(commandBuffer, 0, 2, vertBuffers, offsets);

	// Bind index buffer.
	vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);
}

VkBuffer& Mesh::VertexBuffer() 
{
	return m_vertexBuffer;
}

VkBuffer& Mesh::IndexBuffer() 
{
	return m_indexBuffer;
}

unsigned int Mesh::VertexCount() 
{
	return m_totalVertexCount;
}

unsigned int Mesh::IndexCount() 
{
	return m_totalIndexCount;
}

const std::string& Mesh::GetName() 
{
	return m_name;
}

const VertexInfo* Mesh::VertexFormat() 
{
	return m_vertexFormat;
}

void Mesh::RecordCopyCommandBuffer(Renderer* renderer, VkCommandBuffer cmdBuffer, VkBuffer vertStagingBuffer, VkBuffer vertFinalBuffer, VkBuffer indStagingBuffer, VkBuffer indFinalBuffer, unsigned long long vertCopySize, unsigned long long indCopySize)
{
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	beginInfo.pInheritanceInfo = nullptr;
	beginInfo.pNext = nullptr;

	// Begin recording.
	RENDERER_SAFECALL(vkBeginCommandBuffer(cmdBuffer, &beginInfo), "Mesh Error: Failed to begin recording of copy command buffer.");

	VkBufferCopy vertCopyRegion = {};
	vertCopyRegion.srcOffset = 0;
	vertCopyRegion.dstOffset = 0;
	vertCopyRegion.size = vertCopySize;

	// Copy vertex staging buffer contents to vertex final buffer contents.
	vkCmdCopyBuffer(cmdBuffer, vertStagingBuffer, vertFinalBuffer, 1, &vertCopyRegion);

	VkBufferCopy indCopyRegion = {};
	indCopyRegion.srcOffset = 0;
	indCopyRegion.dstOffset = 0;
	indCopyRegion.size = indCopySize;

	// Copy index staging buffer contents to index final buffer contents.
	vkCmdCopyBuffer(cmdBuffer, indStagingBuffer, indFinalBuffer, 1, &indCopyRegion);

	// End recording.
	RENDERER_SAFECALL(vkEndCommandBuffer(cmdBuffer), "Mesh Error: Failed to end copy command buffer recording.");
}

void Mesh::CalculateTangents(DynamicArray<ComplexVertex>& vertices, DynamicArray<unsigned int>& indices) 
{
	uint32_t nIndexCount = indices.GetSize();

	for (uint32_t i = 0; i < nIndexCount; i += 3) // Step 3 indices, as this should run once per triangle
	{
		// Tangents must be calculated per triangle. We find the correct vertices per tri by using the indices.

		// Positions
		glm::vec3 pos1 = vertices[indices[i]].m_position;
		glm::vec3 pos2 = vertices[indices[i + 1]].m_position;
		glm::vec3 pos3 = vertices[indices[i + 2]].m_position;

		// Tex coords
		glm::vec2 tex1 = vertices[indices[i]].m_texCoords;
		glm::vec2 tex2 = vertices[indices[i + 1]].m_texCoords;
		glm::vec2 tex3 = vertices[indices[i + 2]].m_texCoords;

		// Delta positions
		glm::vec3 dPos1 = pos2 - pos1;
		glm::vec3 dPos2 = pos3 - pos1;

		// Delta tex coords
		glm::vec2 dTex1 = tex2 - tex1;
		glm::vec2 dTex2 = tex3 - tex1;

		float f = 1.0f / (dTex1.x * dTex2.y - dTex2.x * dTex1.y);

		// Calculate tangent only. The bitangent will be calculated in the vertex shader.
		glm::vec4 tangent;
		tangent.x = f * (dTex2.y * dPos1.x - dTex1.y * dPos2.x);
		tangent.y = f * (dTex2.y * dPos1.y - dTex1.y * dPos2.y);
		tangent.z = f * (dTex2.y * dPos1.z - dTex1.y * dPos2.z);
		tangent = glm::normalize(tangent);

		// Assign tangents to vertices.
		vertices[indices[i]].m_tangent = tangent;
		vertices[indices[i + 1]].m_tangent = tangent;
		vertices[indices[i + 2]].m_tangent = tangent;
	}
}

void Mesh::LoadOBJ(DynamicArray<ComplexVertex>& vertices, DynamicArray<unsigned int>& indices, const char* path) 
{
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string errorMessage;

	// Load meshes and materials from the OBJ file.
	bool bLoadSuccess = tinyobj::LoadObj(shapes, materials, errorMessage, path, nullptr);

	if (!bLoadSuccess)
	{
		std::cout << "Mesh Error: Error loading OBJ: " + errorMessage << std::endl;
		return;
	}

	int chunkCount = static_cast<int>(shapes.size());

	size_t nTotalVertexCount = 0;
	size_t nTotalIndexCount = 0;

	// Determine size of whole mesh and allocate memory.
	for (int i = 0; i < chunkCount; ++i)
	{
		nTotalVertexCount += shapes[i].mesh.positions.size();
		nTotalIndexCount += shapes[i].mesh.indices.size();
	}

	vertices.SetSize(static_cast<uint32_t>(nTotalVertexCount));
	indices.SetSize(static_cast<uint32_t>(nTotalIndexCount));

	for (int i = 0; i < chunkCount; ++i)
	{
		tinyobj::shape_t& shape = shapes[i];

		DynamicArray<unsigned int> chunkIndices(static_cast<int>(shape.mesh.indices.size()), 1);
		chunkIndices.SetCount(chunkIndices.GetSize());
		memcpy_s(chunkIndices.Data(), sizeof(unsigned int) * chunkIndices.GetSize(), shape.mesh.indices.data(), sizeof(unsigned int) * shape.mesh.indices.size());

		// Append chunk indices to whole mesh indices...
		if(i == 0) // The indices can be copied for the first submesh, since they do not need to be offset by vertex count.
		{
			indices.SetSize(indices.Count() + static_cast<unsigned int>(shape.mesh.indices.size()));

			int chunkIndicesSize = sizeof(unsigned int) * static_cast<unsigned int>(shape.mesh.indices.size());
			memcpy_s(&indices.Data()[indices.Count()], chunkIndicesSize, shape.mesh.indices.data(), chunkIndicesSize);

			indices.SetCount(indices.GetSize());
		}
		else // Other wise they need to be pushed one by one with the vertex count offset.
		{
			for (int i = 0; i < shape.mesh.indices.size(); ++i)
				indices.Push(shape.mesh.indices[i] + vertices.Count());
		}

		// Set up chunk vertices and add to whole mesh vertices.
		DynamicArray<ComplexVertex> chunkVertices;
		chunkVertices.SetSize(static_cast<int>(shape.mesh.positions.size() / 3)); // Divide size by 3 to account for the fact that the array is of floats rather than vector structs.
		chunkVertices.SetCount(chunkVertices.GetSize());

		for (uint32_t j = 0; j < chunkVertices.Count(); ++j)
		{
			// Positions, normals etc are stored in float format, in groups. (3 for positions and normals, 2 for tex coords).
			// Multiply the index to jump to the current float group.
			int nIndex = j * 3; // Three floats long for positions and normals.
			int nTexIndex = j * 2; // Two floats long for texture coordinates.

			// Copy positions...
			if (shape.mesh.positions.size())
				chunkVertices[i].m_position = glm::vec4(shape.mesh.positions[nIndex], shape.mesh.positions[nIndex + 1], shape.mesh.positions[nIndex + 2], 1.0f);

			// Copy normals...
			if (shape.mesh.normals.size())
				chunkVertices[i].m_normal = glm::vec4(shape.mesh.normals[nIndex], shape.mesh.normals[nIndex + 1], shape.mesh.normals[nIndex + 2], 1.0f);

			// Copy texture coordinates.
			if (shape.mesh.texcoords.size())
				chunkVertices[i].m_texCoords = glm::vec2(shape.mesh.texcoords[nTexIndex], 1 - shape.mesh.texcoords[nTexIndex + 1]);

			// Add the vertex to the whole mesh vertex array.
			vertices.Push(chunkVertices[i]);
		}

		// Calculate tangents for each vertex...
		CalculateTangents(chunkVertices, chunkIndices);
	}

	// Calculate tangents for whole mesh...
	CalculateTangents(vertices, indices);
}