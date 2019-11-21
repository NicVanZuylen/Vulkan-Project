#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (set = 0, binding = 0) uniform UniformBuffer 
{
	mat4 view;
	mat4 proj;
	vec4 viewPos;
} mvp;

layout(input_attachment_index = 0, set = 1, binding = 0) uniform subpassInput inputs[3];

#define BRIGHTNESS_MULT 3
#define SPECULAR_EXPONENT 8
#define SPECULAR_POWER 5

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec4 finalLightPosition;
layout(location = 1) in vec3 finalLightColor;
layout(location = 2) in float finalLightRadius;

void main() 
{
    // Final color output.
    vec4 color = subpassLoad(inputs[0]).rgba;
    vec4 position = subpassLoad(inputs[1]).rgba;
    vec4 normal = subpassLoad(inputs[2]).xyzw;

    // Get direction and color of the current light.
    vec3 posDiff = (position - finalLightPosition).xyz;
    vec3 lightDir = normalize(posDiff).xyz;
    float dist = length(posDiff);

    // Get camera view direction and reflect the light direction upon the normal.
    vec3 viewDir = normalize(mvp.viewPos.xyz - position.xyz);
    vec3 lightReflected = reflect(lightDir, normal.xyz);
        
    // Calculate specular term.
    float specTerm = max(dot(lightReflected, viewDir), 0.0f);

    // Calculate light intensity.
    float normalDotLight = max(-dot(lightDir, normal.xyz), 0.0f);

    // Attenuation function
    float attenuation = -pow((dist / finalLightRadius) + 0.1f, 3) + 1;

    // Add to final lighting.
    vec3 lighting = normalDotLight * finalLightColor * BRIGHTNESS_MULT; // Lighting component.
    lighting += pow(specTerm, SPECULAR_EXPONENT) * SPECULAR_POWER * finalLightColor;

    outColor = vec4(color.rgb * lighting * attenuation, 1.0f); // Multiply color by lighting level as output.
}

