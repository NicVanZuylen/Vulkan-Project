#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include "DynamicArray.h"

class Renderer;
class MeshRenderer;
struct PipelineInfo;

struct Shader 
{
	/*
	Description: Construct a shader without loading shader code.
	*/
	Shader();

	/*
	Description: Create and load a shader with stages loaded from the provided file paths.
	Param:
	    const char* vertPath: Path to the SPIR-V code of the vertex shader stage. 
		const char* fragPath: Path to the SPIR-V code of the fragment shader stage.
	*/
	Shader(const char* vertPath, const char* fragPath);

	/*
	Description: Create, name and load a shader with stages loaded from the provided file paths.
	Param:
	    const char* name: The name used to identify the shader.
		const char* vertPath: Path to the SPIR-V code of the vertex shader stage.
		const char* fragPath: Path to the SPIR-V code of the fragment shader stage.
	*/
	Shader(const char* name, const char* vertPath, const char* fragPath);

	/*
	Description: Load new SPIR-V shader code from the provided paths.
	Param:
	    const char* vertPath: Path to the SPIR-V code of the vertex shader stage.
		const char* fragPath: Path to the SPIR-V code of the fragment shader stage.
	*/
	void Load(const char* vertPath, const char* fragPath);

	std::string m_name;
	DynamicArray<char> m_vertContents;
	DynamicArray<char> m_fragContents;

	VkShaderModule m_vertModule;
	VkShaderModule m_fragModule;

    PipelineInfo* m_pipeline;

	// All objects that use this shader/pipeline.
	DynamicArray<MeshRenderer*> m_dependentObjects;

	bool m_registered;
};
