#include "VertexInfo.h"

VertexInfo::VertexInfo()
{
	m_nameID = "EMPTY_FORMAT";
}

VertexInfo::VertexInfo(const DynamicArray<EVertexAttribute>& attributes)
{
	m_attributes = attributes;
	m_nameID = "EMPTY_FORMAT";

	CalculateInputInformation();
}

VertexInfo::~VertexInfo()
{

}

void VertexInfo::SetAttributes(const DynamicArray<EVertexAttribute>& attributes) 
{
	m_attributes = attributes;

	CalculateInputInformation();
}

void VertexInfo::operator=(const std::initializer_list<EVertexAttribute> attributes) 
{
	m_attributes = attributes;

	CalculateInputInformation();
}

VkVertexInputBindingDescription VertexInfo::BindingDescription() const
{
	return m_bindDescription;
}

const DynamicArray<VkVertexInputAttributeDescription>& VertexInfo::AttributeDescriptions() const
{
	return m_attribDescriptions;
}

const std::string& VertexInfo::NameID() const
{
	return m_nameID;
}

void VertexInfo::CalculateInputInformation()
{
	m_bindDescription.binding = 0;
	m_bindDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; // Data is per-vertex rather than per-instance.

	// Make sure the attribute description array is clear.
	m_attribDescriptions.Clear();

	uint32_t& currentOffset = m_bindDescription.stride; // Will be used for stride when it's complete.

	m_nameID = "|";

	for (int i = 0; i < m_attributes.Count(); ++i)
	{
		VkVertexInputAttributeDescription desc = {};
		desc.binding = 0;
		desc.location = i; // Location is index.
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
		m_attribDescriptions.Push(desc);
	}

	m_nameID += "|";
}

