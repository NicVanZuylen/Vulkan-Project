#include "VertexInfo.h"
#include <iostream>

VertexInfo::VertexInfo()
{
	m_nameID = "EMPTY_FORMAT";
	m_attribDescriptions = nullptr;
}

VertexInfo::VertexInfo(const DynamicArray<EVertexAttribute>& attributes, bool bPerInstance, const VertexInfo* prevBufferInfo)
{
	m_attributes = attributes;
	m_nameID = "EMPTY_FORMAT";
	m_attribDescriptions = nullptr;

	CalculateInputInformation(bPerInstance, prevBufferInfo);
}

VertexInfo::~VertexInfo()
{
	delete[] m_attribDescriptions;
}

void VertexInfo::SetAttributes(const DynamicArray<EVertexAttribute>& attributes, bool bPerInstance, const VertexInfo* prevBufferInfo)
{
	m_attributes = attributes;

	CalculateInputInformation(bPerInstance, prevBufferInfo);
}

VkVertexInputBindingDescription VertexInfo::BindingDescription() const
{
	return m_bindDescription;
}

int VertexInfo::AttributeDescriptionCount() const
{
	return m_attributes.Count();
}

const VkVertexInputAttributeDescription* VertexInfo::AttributeDescriptions() const
{
	return m_attribDescriptions;
}

const std::string& VertexInfo::NameID() const
{
	return m_nameID;
}

void VertexInfo::CalculateInputInformation(const bool& bPerInstance, const VertexInfo* prevBufferInfo)
{
	if (m_attribDescriptions)
		delete[] m_attribDescriptions;

	m_attribDescriptions = new VkVertexInputAttributeDescription[m_attributes.Count()];

	m_bindDescription.inputRate = static_cast<VkVertexInputRate>(bPerInstance); // Data is per-vertex rather than per-instance.
	m_bindDescription.binding = 0;

	if(prevBufferInfo)
	    m_bindDescription.binding = prevBufferInfo->m_bindDescription.binding + 1;

	uint32_t& currentOffset = m_bindDescription.stride; // Will be used for stride when it's complete.

	m_nameID = "|";

	for (int i = 0; i < m_attributes.Count(); ++i)
	{
		VkVertexInputAttributeDescription desc = {};
		desc.binding = m_bindDescription.binding;
		desc.location = i; // Location is index.

		if(prevBufferInfo) 
		{
			// Location is the previous buffer's attribute count + i.
			desc.location = prevBufferInfo->AttributeDescriptionCount() + i;
		}

		desc.offset = currentOffset;

		// Switch the current attribute and add the appropriate size to the offset and set the appropriate attribute format.
		switch (m_attributes[i])
		{
		case VERTEX_ATTRIB_FLOAT:

			m_nameID += "FLOAT";
			desc.format = VK_FORMAT_R32_SFLOAT;
			currentOffset += sizeof(float);
			break;

		case VERTEX_ATTRIB_FLOAT2:

			m_nameID += "FLOAT2";
			desc.format = VK_FORMAT_R32G32_SFLOAT;
			currentOffset += sizeof(float) * 2;
			break;

		case VERTEX_ATTRIB_FLOAT3:

			m_nameID += "FLOAT3";
			desc.format = VK_FORMAT_R32G32B32_SFLOAT;
			currentOffset += sizeof(float) * 3;
			break;

		case VERTEX_ATTRIB_FLOAT4:

			m_nameID += "FLOAT4";
			desc.format = VK_FORMAT_R32G32B32A32_SFLOAT;
			currentOffset += sizeof(float) * 4;
			break;

		case VERTEX_ATTRIB_MAT2:

			m_nameID += "MAT2";
			desc.format = VK_FORMAT_R32G32_SFLOAT;
			currentOffset += sizeof(float) * 4;
			break;

		case VERTEX_ATTRIB_MAT3:

			m_nameID += "MAT3";
			desc.format = VK_FORMAT_R32G32B32_SFLOAT;
			currentOffset += sizeof(float) * 9;
			break;

		case VERTEX_ATTRIB_MAT4:

			m_nameID += "MAT4";
			desc.format = VK_FORMAT_R32G32B32A32_SFLOAT;
			currentOffset += sizeof(float) * 16;
			break;

		case VERTEX_ATTRIB_INT:

			m_nameID += "INT";
			desc.format = VK_FORMAT_R32_SINT;
			currentOffset += sizeof(int);
			break;

		case VERTEX_ATTRIB_INT2:

			m_nameID += "INT2";
			desc.format = VK_FORMAT_R32G32_SINT;
			currentOffset += sizeof(int) * 2;
			break;

		case VERTEX_ATTRIB_INT3:

			m_nameID += "INT3";
			desc.format = VK_FORMAT_R32G32B32_SINT;
			currentOffset += sizeof(int) * 3;
			break;

		case VERTEX_ATTRIB_INT4:

			m_nameID += "INT4";
			desc.format = VK_FORMAT_R32G32B32A32_SINT;
			currentOffset += sizeof(int) * 4;
			break;
		}

		// Add to output.
		m_attribDescriptions[i] = desc;
	}

	m_nameID += "|";
}

