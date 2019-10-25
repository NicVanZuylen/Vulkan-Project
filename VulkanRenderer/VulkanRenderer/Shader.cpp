#include "Shader.h"
#include "Renderer.h"
#include "RenderObject.h"

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

Shader::Shader(Renderer* renderer, const char* vertPath, const char* fragPath)
{
	m_renderer = renderer;

	m_vertModule = nullptr;
	m_fragModule = nullptr;

	m_registered = false;

	std::string vertStr = vertPath;
	std::string fragStr = fragPath;

	// Name is the name of the fragment shader file appended to the name of the vertex shader file.
	m_name = vertStr.substr(vertStr.find_last_of('/') + 1) + "|" + fragStr.substr(fragStr.find_last_of('/') + 1);

	// Detect if the files are SPIR-V or raw GLSL.
	std::string vertExtension = vertPath;
	vertExtension = vertExtension.substr(vertExtension.find_last_of('.') + 1);

	std::string fragExtension = fragPath;
	fragExtension = fragExtension.substr(fragExtension.find_last_of('.') + 1);

	// Check if files are SPIR-V files.
	bool bRawFiles = std::strcmp(vertExtension.c_str(), "spv") != 0 && std::strcmp(fragExtension.c_str(), "spv") != 0;

	if (!bRawFiles) 
	{
		DynamicArray<char> vertContents;
		DynamicArray<char> fragContents;

		Load(vertPath, fragPath, vertContents, fragContents);

		CreateModules(vertContents, fragContents);
	}
	else
	{
		std::string vertContents = LoadRaw(vertPath);
		std::string fragContents = LoadRaw(fragPath);
	}
}

Shader::Shader(Renderer* renderer, const char* name, const char* vertPath, const char* fragPath)
{
	m_renderer = renderer;

	m_name = name;

	m_vertModule = nullptr;
	m_fragModule = nullptr;

	m_registered = false;

	// Detect if the files are SPIR-V or raw GLSL.
	std::string vertExtension = vertPath;
	vertExtension = vertExtension.substr(vertExtension.find_last_of('.') + 1);

	std::string fragExtension = fragPath;
	fragExtension = fragExtension.substr(fragExtension.find_last_of('.') + 1);

	// Check if files are now SPIR-V files.
	bool bRawFiles = std::strcmp(vertExtension.c_str(), "spv") != 0 && std::strcmp(fragExtension.c_str(), "spv") != 0;

	if (!bRawFiles) 
	{
		DynamicArray<char> vertContents;
		DynamicArray<char> fragContents;

		Load(vertPath, fragPath, vertContents, fragContents);

		CreateModules(vertContents, fragContents);
	}
	else 
	{
		std::string vertContents = LoadRaw(vertPath);
		std::string fragContents = LoadRaw(fragPath);
	}
}

void Shader::Load(const char* vertPath, const char* fragPath, DynamicArray<char>& vertContents, DynamicArray<char>& fragContents)
{
	if (m_name == "UNNAMED_SHADER")
	{
		std::string vertStr = vertPath;
		std::string fragStr = fragPath;

		m_name = vertStr.substr(vertStr.find_last_of('/') + 1) + "|" + fragStr.substr(fragStr.find_last_of('/') + 1);
	}

	// -----------------------------------------------------------------------------------------------
	// Vertex shader

	// Start at the end of the file so that tellg() returns the size of the file.
	std::ifstream vertFile(vertPath, std::ios::binary | std::ios::ate);

	int vertExcept = vertFile.exceptions();

	if (!vertExcept && vertFile.good())
	{
		// Get file size.
		const int fileSize = (const int)vertFile.tellg();

		// Allocate space for the file contents.
		vertContents.SetSize(fileSize);

		// Return to the start of the file and read it.
		vertFile.seekg(0);
		vertFile.read(vertContents.Data(), fileSize);

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

	if (!fragExcept && fragFile.good())
	{
		// Get file size.
		const int fileSize = (const int)fragFile.tellg();

		// Allocate space for the file contents.
		fragContents.SetSize(fileSize);

		// Return to the start of the file and read it.
		fragFile.seekg(0);
		fragFile.read(fragContents.Data(), fileSize);

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

Shader::~Shader() 
{
	// Destroy shader modules if they exist.
	if(m_vertModule) 
	{
		vkDestroyShaderModule(m_renderer->GetDevice(), m_vertModule, nullptr);
	}
	if(m_fragModule) 
	{
		vkDestroyShaderModule(m_renderer->GetDevice(), m_fragModule, nullptr);
	}
}

std::string Shader::LoadRaw(const char* path) 
{
	// Open files and seek to end.
	std::ifstream file(path, std::ios::in);
	std::stringstream inStream;
	std::string contents;

	// Ensure file was successfully opened, and is not empty.
	if (file.good())
	{
		// Seek to beginning of the file.
		file.seekg(0);

		// Read contents.
		inStream << file.rdbuf();

		// Release file handle.
		file.close();

		// Copy contents to string.
		contents = inStream.str();

		return contents;

		std::cout << "Successfully read GLSL source file at: " << path << "\n";
	}
	else
		std::cout << "Failed to read GLSL source file at: " << path << "\n";

	contents = "FAIL_STRING";

	return contents;
}

void Shader::CreateModules(DynamicArray<char>& vertContents, DynamicArray<char>& fragContents) 
{
	// Create shader modules for the vertex and fragment shaders.
	VkShaderModuleCreateInfo modCreateInfo = {};
	modCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	modCreateInfo.codeSize = vertContents.GetSize();
	modCreateInfo.pCode = reinterpret_cast<const uint32_t*>(vertContents.Data());

	RENDERER_SAFECALL(vkCreateShaderModule(m_renderer->GetDevice(), &modCreateInfo, nullptr, &m_vertModule), "Shader Error: Failed to create vertex shader module.");

	modCreateInfo.codeSize = fragContents.GetSize();
	modCreateInfo.pCode = reinterpret_cast<const uint32_t*>(fragContents.Data());

	RENDERER_SAFECALL(vkCreateShaderModule(m_renderer->GetDevice(), &modCreateInfo, nullptr, &m_fragModule), "Shader Error: Failed to create fragment shader module.");
}

/*
void Shader::CompileGLSL(const char* contents, const char* path, EShaderStage eStage, const char** includeDirs, unsigned int nIncludeDirCount)
{
	// Initialize glslang.
	glslang::InitializeProcess();

	EShLanguage type;

	switch (eStage) 
	{
	case SHADER_VERTEX:

		type = EShLanguage::EShLangVertex;

		break;

	case SHADER_GEOMETRY:

		type = EShLanguage::EShLangGeometry;

		break;

	case SHADER_FRAGMENT:

		type = EShLanguage::EShLangFragment;

		break;
	}

	// Create shader stage to compile.
	glslang::TShader shader(type);

	shader.setStrings(&contents, 1);

	int nClientInputSemanticsVer = 100; // Effectively #define VULKAN 100 for Vulkan 1.0
	glslang::EShTargetClientVersion vulkanClientVer = glslang::EShTargetVulkan_1_0;
	glslang::EShTargetLanguageVersion langVersion = glslang::EShTargetSpv_1_0;

	// Set Vulkan version, source code info & SPIR-V target details.
	shader.setEnvInput(glslang::EShSourceGlsl, type, glslang::EShClientVulkan, nClientInputSemanticsVer);
	shader.setEnvClient(glslang::EShClientVulkan, vulkanClientVer);
	shader.setEnvTarget(glslang::EShTargetLanguage::EShTargetSpv, langVersion);

	TBuiltInResource resources = {};
	EShMessages messages = static_cast<EShMessages>(EShMessages::EShMsgSpvRules | EShMessages::EShMsgVulkanRules); // Output messages regarding SPIR-V & Vulkan.

	// Can include external GLSL source files.
	DirStackFileIncluder includer;

	// Push include directories.
	for(unsigned int i = 0; i < nIncludeDirCount; ++i) 
	    includer.pushExternalLocalDirectory(includeDirs[i]);

	std::string preprocessedGLSL;

	// Preprocessing
	if(!shader.preprocess(&resources, 100, EProfile::ENoProfile, false, false, messages, &preprocessedGLSL, includer)) 
	{
		// Output errors.
		std::cout << "GLSL preproccessing failed for file: " << path << "\n";
		std::cout << shader.getInfoLog() << "\n";
		std::cout << shader.getInfoDebugLog() << "\n";
	}

	const char* preprocessedCString = preprocessedGLSL.c_str();

	// Update strings.
	shader.setStrings(&preprocessedCString, 1);

	// Parse shader...
	glslang::TProgram program;
	program.addShader(&shader);

	if (!program.link(messages)) 
	{
		std::cout << "GLSL linking failed for file: " << path << "\n";
		std::cout << program.getInfoLog() << "\n";
		std::cout << program.getInfoDebugLog() << "\n";
	}

	spv::SpvBuildLogger spvLogger;
	glslang::SpvOptions spvOptions;

	std::vector<unsigned int> spvContents;

	glslang::GlslangToSpv(*program.getIntermediate(type), spvContents, &spvOptions);
}
*/