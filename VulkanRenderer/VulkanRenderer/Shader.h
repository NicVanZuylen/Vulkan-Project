#pragma once
#include <string>
#include "DynamicArray.h"

class Renderer;
class MeshRenderer;
struct PipelineData;
struct VkShaderModule_T;

enum EShaderStage 
{
	SHADER_VERTEX,
	SHADER_GEOMETRY,
	SHADER_FRAGMENT
};

struct Shader 
{
	/*
	Description: Construct a shader without loading shader code.
	*/
	Shader();

	/*
	Description: Create and load a shader with stages loaded from the provided file paths.
	Param:
	    Renderer* renderer: The renderer that will use this shader.
	    const char* vertPath: Path to the SPIR-V code of the vertex shader stage. 
		const char* fragPath: Path to the SPIR-V code of the fragment shader stage.
	*/
	Shader(Renderer* renderer, const char* vertPath, const char* fragPath);

	/*
	Description: Create, name and load a shader with stages loaded from the provided file paths.
	Param:
	    Renderer* renderer: The renderer that will use this shader.
	    const char* name: The name used to identify the shader.
		const char* vertPath: Path to the SPIR-V code of the vertex shader stage.
		const char* fragPath: Path to the SPIR-V code of the fragment shader stage.
	*/
	Shader(Renderer* renderer, const char* name, const char* vertPath, const char* fragPath);

	~Shader();

	/*
	Description: Load new SPIR-V shader code from the provided paths.
	Param:
	    const char* vertPath: Path to the SPIR-V code of the vertex shader stage.
		const char* fragPath: Path to the SPIR-V code of the fragment shader stage.
		DynamicArray<char>& vertContents: Vertex Shader SPIR-V content output.
		DynamicArray<char>& fragContents: Fragment Shader SPIR-V content output.
	*/
	void Load(const char* vertPath, const char* fragPath, DynamicArray<char>& vertContents, DynamicArray<char>& fragContents);

	/*
	Description: Load raw GLSL source from the provided file path.
	Return Type: string
	Param:
		const char* path: Path to the GLSL source file.
	*/
	std::string LoadRaw(const char* path);

	/*
	Description: Create shader modules for the shader stages.
	Param:
	    DynamicArray<char>& vertContents: Vertex Shader SPIR-V contents.
	    DynamicArray<char>& fragContents: Fragment Shader SPIR-V contents.
	*/
	void CreateModules(DynamicArray<char>& vertContents, DynamicArray<char>& fragContents);

	/*
	Description: Compile Raw GLSL source into SPIR-V & use the compiled SPIR-V output.
	Param:
	    const char* contents: GLSL source to compile.
		const char* path: filepath to the GLSL source file to compile.
		EShaderStage eStage: The shader stage to compile for.
		const char** includeDirs: Array of include directory strings.
		unsigned int nIncludeDirCount: Amount of include directories in the array.
	*/
	void CompileGLSL(const char* contents, const char* path, EShaderStage eStage, const char** includeDirs, unsigned int nIncludeDirCount);

	Renderer* m_renderer;

	std::string m_name;
	//DynamicArray<char> m_vertContents;
	//DynamicArray<char> m_fragContents;

	VkShaderModule_T*  m_vertModule;
	VkShaderModule_T*  m_fragModule;

	bool m_registered;
};
