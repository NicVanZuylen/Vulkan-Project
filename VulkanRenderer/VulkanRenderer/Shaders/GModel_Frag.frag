#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outPos;
layout(location = 2) out vec4 outNormal;

layout(binding = 1) uniform sampler2D textures[2];

layout(location = 0) in vec4 finalPos;
layout(location = 1) in vec4 finalNormal;
layout(location = 2) in vec2 finalTexCoords;

void main() 
{
    // Color G Buffer output.
    outColor = texture(textures[0], finalTexCoords) * texture(textures[1], finalTexCoords);

    // Position G Buffer ouput.
    outPos = finalPos;

    // Normal G Buffer output.
    outNormal = finalNormal;
}

