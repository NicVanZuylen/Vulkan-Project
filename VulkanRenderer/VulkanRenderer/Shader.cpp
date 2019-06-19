#include "Shader.h"
#include "Renderer.h"
#include <iostream>
#include <fstream>
#include <sstream>

/*
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
*/

Shader::Shader() 
{
	m_name = "UNNAMED_SHADER";

	m_vertModule = nullptr;
	m_fragModule = nullptr;
}

Shader::Shader(const char* vertPath, const char* fragPath) 
{
	m_vertModule = nullptr;
	m_fragModule = nullptr;

	std::string vertStr = vertPath;
	std::string fragStr = fragPath;

	m_name = vertStr.substr(vertStr.find_last_of('/')) + fragStr.substr(fragStr.find_last_of('/'));

	Load(vertPath, fragPath);
}

Shader::Shader(const char* name, const char* vertPath, const char* fragPath)
{
	m_name = name;

	m_vertModule = nullptr;
	m_fragModule = nullptr;

	Load(vertPath, fragPath);
}

void Shader::Load(const char* vertPath, const char* fragPath) 
{
	if (m_name == "UNNAMED_SHADER")
	{
		std::string vertStr = vertPath;
		std::string fragStr = fragPath;

		m_name = vertStr.substr(vertStr.find_last_of('/')) + fragStr.substr(fragStr.find_last_of('/'));
	}

	// -----------------------------------------------------------------------------------------------
	// Vertex shader

	// Start at the end of the file so that tellg() returns the size of the file.
	std::ifstream vertFile(vertPath, std::ios::binary | std::ios::ate);

	int vertExcept = vertFile.exceptions();

	if (!vertExcept)
	{
		// Get file size.
		const int fileSize = (const int)vertFile.tellg();

		// Allocate space for the file contents.
		m_vertContents.SetSize(fileSize);

		// Return to the start of the file and read it.
		vertFile.seekg(0);
		vertFile.read(m_vertContents.Data(), fileSize);

		// Close file.
		vertFile.close();

		std::cout << "Successfully read vertex shader file at: " << vertPath << std::endl;
	}
	else
	{
		// File was not opened.
		std::cout << "Failed to open vertex shader file at: " << vertPath << std::endl;
	}

	// -----------------------------------------------------------------------------------------------
	// Fragment shader

	// Start at the end of the file so that tellg() returns the size of the file.
	std::ifstream fragFile(fragPath, std::ios::binary | std::ios::ate);

    int fragExcept = fragFile.exceptions();

	if (!fragExcept)
	{
		// Get file size.
		const int fileSize = (const int)fragFile.tellg();

		// Allocate space for the file contents.
		m_fragContents.SetSize(fileSize);

		// Return to the start of the file and read it.
		fragFile.seekg(0);
		fragFile.read(m_fragContents.Data(), fileSize);

		// Close file.
		fragFile.close();

		std::cout << "Successfully read fragment shader file at: " << fragPath << std::endl;
	}
	else
	{
		// File was not opened.
		std::cout << "Failed to open fragment shader file at: " << fragPath << std::endl;
	}

	// -----------------------------------------------------------------------------------------------
}