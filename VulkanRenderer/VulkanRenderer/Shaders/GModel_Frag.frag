#version 450
#extension GL_ARB_separate_shader_objects : enable
//#extension GL_KHR_vulkan_glsl : enable

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outPos;
layout(location = 2) out vec4 outNormal;
layout(location = 3) out vec4 outEmission;

layout(set = 1, binding = 0) uniform Properties 
{
    vec4 colorTint;
} properties;

layout(set = 1, binding = 1) uniform sampler2D textures[3];

layout(location = 0) in vec4 f_finalPos;
layout(location = 1) in vec2 f_texCoords;
layout(location = 2) in mat3 f_tbn;

void main() 
{
    // Color G Buffer output.
    outColor = texture(textures[0], f_texCoords) * properties.colorTint;

    // Position G Buffer ouput.
    outPos = f_finalPos;

    // Normal G Buffer output.
    outNormal = vec4(normalize(f_tbn * (texture(textures[1], f_texCoords).xyz * 2.0f - 1.0f)), 1.0f);

	// Emissive output.
	outEmission = texture(textures[2], f_texCoords);
}

