#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (set = 0, binding = 0) uniform ShadowMapCamera 
{
	mat4 view;
	mat4 proj;
} camera;

layout(location = 0) in vec4 position;
layout(location = 1) in vec4 normal;
layout(location = 2) in vec4 tangent;
layout(location = 3) in vec2 texCoords;
layout(location = 4) in mat4 model;

void main() 
{
	// Transform vertex.
    gl_Position = camera.proj * camera.view * model * position;
}