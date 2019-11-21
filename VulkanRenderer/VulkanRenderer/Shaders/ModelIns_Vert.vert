#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (set = 0, binding = 0) uniform UniformBuffer 
{
	mat4 view;
	mat4 proj;
	vec4 viewPos;
} mvp;

layout(location = 0) in vec4 position;
layout(location = 1) in vec4 normal;
layout(location = 2) in vec4 tangent;
layout(location = 3) in vec2 texCoords;
layout(location = 4) in mat4 model;

layout(location = 0) out vec4 finalPos;
layout(location = 1) out vec4 finalNormal;
layout(location = 2) out vec2 finalTexCoords;

void main() 
{
    finalPos = model * position;
    finalNormal = model * normal; // Transform mesh normal and pass to the next shader stage.
    finalTexCoords = texCoords;

    gl_Position = mvp.proj * mvp.view * model * position;
}