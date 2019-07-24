#pragma once
#include "DynamicArray.h"
#include <string>
#include <vulkan/vulkan.h>

enum EVertexAttribute 
{
	VERTEX_ATTRIB_FLOAT,
	VERTEX_ATTRIB_FLOAT2,
	VERTEX_ATTRIB_FLOAT3,
	VERTEX_ATTRIB_FLOAT4,
	VERTEX_ATTRIB_INT,
	VERTEX_ATTRIB_INT2,
	VERTEX_ATTRIB_INT3,
	VERTEX_ATTRIB_INT4
};

class VertexInfo
{
public:

	VertexInfo();

	/*
	Constructor:
	Param:
		const DynamicArray<EVertexAttribute>& attributes: Array of attributes used to create the format.
		bool bPerInstance: Whether or not the vertex data is input per vertex or per instance.
		const unsigned int& nBinding: The vertex binding index.
	*/
	VertexInfo(const DynamicArray<EVertexAttribute>& attributes, bool bPerInstance = false, const VertexInfo* prevBufferInfo = nullptr);
	
	~VertexInfo();

	/*
	Description: Set the attibutes of this format.
	Param:
	    const DynamicArray<EVertexAttribute>& attributes: Array of attributes used to create the format.
		bool bPerInstance: Whether or not the vertex data is input per vertex or per instance.
	*/
	void SetAttributes(const DynamicArray<EVertexAttribute>& attributes, bool bPerInstance = false, const VertexInfo* prevBufferInfo = nullptr);

	/*
	Description: Return the vertex input binding description for this format.
	Return Type: VkVertexInputBindingDescription&
	*/
	VkVertexInputBindingDescription BindingDescription() const;

	/*
	Description: The amount of vertex attribute descriptions.
	Return Type: int
	*/
	int AttributeDescriptionCount() const;

	/*
	Description: Return the vertex input attribute descriptions for this format.
	Return Type: const DynamicArray<VkVertexInputAttributeDescription>&
	*/
	const VkVertexInputAttributeDescription* AttributeDescriptions() const;

	/*
	Description: Get the name ID of this vertex format.
	*/
	const std::string& NameID() const;

private:

	DynamicArray<EVertexAttribute> m_attributes;

	VkVertexInputBindingDescription m_bindDescription;
	VkVertexInputAttributeDescription* m_attribDescriptions;

	std::string m_nameID;

	/*
	Description: Calculate the binding description and attirbute descriptions for this vertex format.
	*/
	void CalculateInputInformation(const bool& bPerInstance, const VertexInfo* prevBufferInfo);
};

