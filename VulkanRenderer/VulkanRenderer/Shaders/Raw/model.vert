#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (binding = 0) uniform UniformBuffer 
{
    mat4 model;
	mat4 view;
	mat4 proj;
} mvp;

layout(location = 0) in vec4 position;
layout(location = 1) in vec4 normal;
layout(location = 2) in vec4 tangent;
layout(location = 3) in vec2 texCoords;

layout(location = 0) out vec2 finalTexCoords;

void main() 
{
    finalTexCoords = texCoords;
    gl_Position = mvp.proj * mvp.view * mvp.model * position;
}