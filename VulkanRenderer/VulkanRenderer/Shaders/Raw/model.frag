#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler2D textures[2];

layout(location = 0) in vec2 finalTexCoords;

void main() 
{
    outColor = texture(textures[0], finalTexCoords) * texture(textures[1], finalTexCoords);
}

