#pragma once
#include <vulkan/vulkan.h>
#include "DynamicArray.h"

class Renderer;

class Shader 
{
public:

	Shader(const char* vertPath, const char* fragPath);

	~Shader();

	const DynamicArray<char>& VertContents();

	const DynamicArray<char>& FragContents();

private:

	void ReadFile(DynamicArray<char>& buffer, const char* path);

	DynamicArray<char> m_vertContents;
	DynamicArray<char> m_fragContents;

	// The renderer instance this shader is registered with.
	Renderer* m_renderer;
};