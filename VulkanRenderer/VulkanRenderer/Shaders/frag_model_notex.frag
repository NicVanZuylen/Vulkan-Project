#version 450
#extension GL_ARB_separate_shader_objects : enable
//#extension GL_KHR_vulkan_glsl : enable

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outEmission;
layout(location = 3) out vec4 outMatProps; // Per-fragment material properties. (RGB = spec, A = roughness).

layout(set = 1, binding = 0) uniform Properties 
{
    vec4 colorTint;
} properties;

layout(location = 0) in vec4 f_finalPos;
layout(location = 1) in vec2 f_texCoords;
layout(location = 2) in vec4 f_normal;

void main() 
{
    // Color G Buffer output.
    outColor = properties.colorTint;

    // Normal G Buffer output.
    outNormal = vec4(f_normal.xyz, 1.0f);

	// Emissive output.
	outEmission = vec4(0.0f, 0.0f, 0.0f, 1.0f);

	outMatProps = vec4(1.0f, 1.0f, 1.0f, 0.0f);
}

