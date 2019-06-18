#include "Shader.h"
#include "Renderer.h"
#include <iostream>
#include <fstream>
#include <sstream>

Shader::Shader(const char* vertPath, const char* fragPath) 
{
	ReadFile(m_vertContents, vertPath);
	ReadFile(m_fragContents, fragPath);
	
	m_renderer = nullptr;
}

Shader::~Shader() 
{
	
}

const DynamicArray<char>& Shader::VertContents() 
{
	return m_vertContents;
}

const DynamicArray<char>& Shader::FragContents()
{
	return m_fragContents;
}

void Shader::ReadFile(DynamicArray<char>& buffer, const char* path) 
{
	// Start at the end of the file so that tellg() returns the size of the file.
	std::ifstream file(path, std::ios::binary | std::ios::ate);

	int except = file.exceptions();

	if(!except) 
	{
		// Get file size.
		const int fileSize = (const int)file.tellg();

		// Allocate space for the file contents.
		buffer.SetSize(fileSize);

		// Return to the start of the file and read it.
		file.seekg(0);
		file.read(buffer.Data(), fileSize);

		// Close file.
		file.close();

		std::cout << "Successfully read shader file at: " << path << std::endl;
	}
	else 
	{
		// File was not opened.
		std::cout << "Failed to open shader file at: " << path << std::endl;
	}
}
