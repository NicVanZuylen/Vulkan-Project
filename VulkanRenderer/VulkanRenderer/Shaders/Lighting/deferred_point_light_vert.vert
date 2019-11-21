#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (set = 0, binding = 0) uniform UniformBuffer 
{
    mat4 model;
	mat4 view;
	mat4 proj;
	vec4 viewPos;
} mvp;

// Standard mesh format inputs.
layout(location = 0) in vec4 position;
layout(location = 1) in vec4 normal;
layout(location = 2) in vec4 tangent;
layout(location = 3) in vec2 texCoords;

// Light volume instance inputs.
layout(location = 4) in vec4 lightPosition;
layout(location = 5) in vec4 lightColorRadius;

layout(location = 0) out vec4 finalLightPosition;
layout(location = 1) out vec3 finalLightColor;
layout(location = 2) out float finalLightRadius;

void main() 
{
    vec3 finalPos = (position.xyz * lightColorRadius.w) + lightPosition.xyz;

    finalLightPosition = lightPosition;
    finalLightColor = lightColorRadius.rgb;
    finalLightRadius = lightColorRadius.w;

    gl_Position = mvp.proj * mvp.view * vec4(finalPos, 1.0f);
}