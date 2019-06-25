#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 position;
layout(location = 1) in vec4 color;

layout (location = 0) out vec3 fragColor;

void main() 
{
    gl_Position = position;
	fragColor = color.rgb;
}