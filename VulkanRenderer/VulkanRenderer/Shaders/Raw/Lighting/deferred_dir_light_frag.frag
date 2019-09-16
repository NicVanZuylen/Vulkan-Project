#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (set = 0, binding = 0) uniform UniformBuffer 
{
    mat4 model;
	mat4 view;
	mat4 proj;
	vec4 viewPos;
} mvp;

layout(input_attachment_index = 0, set = 1, binding = 0) uniform subpassInput inputs[3];

layout(set = 2, binding = 0) uniform DirectionalLight 
{
    vec4 direction;
    vec4 color;
} dirLights[1];

#define BRIGHTNESS_MULT 3
#define SPECULAR_EXPONENT 8
#define SPECULAR_POWER 5

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec2 finalTexCoords;

void main() 
{
    // Final color output.
    vec4 color = subpassLoad(inputs[0]).rgba;
    vec4 position = subpassLoad(inputs[1]).rgba;
    vec4 normal = subpassLoad(inputs[2]).xyzw;

    vec4 lightDir = dirLights[0].direction;
    vec4 lightColor = dirLights[0].color;

    vec3 viewDir = normalize(mvp.viewPos.xyz - position.xyz);
    vec3 lightReflected = reflect(lightDir.xyz, normal.xyz);

    float specTerm = max(dot(lightReflected, viewDir), 0.0f);

    float normalDotLight = max(-dot(lightDir, normal), 0.0f);

    vec3 lighting = vec3(0.3f); // Ambient component.
    lighting += normalDotLight * lightColor.rgb * BRIGHTNESS_MULT; // Lighting component.
    lighting += pow(specTerm, SPECULAR_EXPONENT) * SPECULAR_POWER * lightColor.rgb;

    outColor = vec4(color.rgb * lighting, 1.0f); // Multiply color by lighting level as output.
}

