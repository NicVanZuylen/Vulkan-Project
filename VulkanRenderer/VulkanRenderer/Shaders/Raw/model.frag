#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler2D diffuse;

layout(location = 0) in vec2 finalTexCoords;

void main() 
{
    outColor = texture(diffuse, finalTexCoords);
}

