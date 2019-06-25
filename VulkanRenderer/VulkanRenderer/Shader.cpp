#include "Shader.h"
#include "Renderer.h"
#include "MeshRenderer.h"
#include <iostream>
#include <fstream>
#include <sstream>

Shader::Shader() 
{
	m_name = "UNNAMED_SHADER";

	m_vertModule = nullptr;
	m_fragModule = nullptr;

	m_registered = false;
}

Shader::Shader(const char* vertPath, const char* fragPath) 
{
	m_vertModule = nullptr;
	m_fragModule = nullptr;

	m_registered = false;

	std::string vertStr = vertPath;
	std::string fragStr = fragPath;

	// Name is the name of the fragment shader file appended to the name of the vertex shader file.
	m_name = vertStr.substr(vertStr.find_last_of('/') + 1) + "|" + fragStr.substr(fragStr.find_last_of('/') + 1);

	Load(vertPath, fragPath);
}

Shader::Shader(const char* name, const char* vertPath, const char* fragPath)
{
	m_name = name;

	m_vertModule = nullptr;
	m_fragModule = nullptr;

	m_registered = false;

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