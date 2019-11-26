#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (set = 0, binding = 0) uniform UniformBuffer 
{
	mat4 view;
	mat4 proj;
	vec4 viewPos;
} mvp;

layout(input_attachment_index = 0, set = 1, binding = 0) uniform subpassInput inputs[4];

#define DIRECTIONAL_LIGHT_COUNT 4

struct GlobalDirLightData 
{
    int count;
    int padding[3];	
};

struct DirectionalLight 
{
	vec4 direction;
    vec4 color;
};

layout(set = 2, binding = 0) uniform DirectionalLightArray 
{
    DirectionalLight data[DIRECTIONAL_LIGHT_COUNT];
    GlobalDirLightData globalData;
} dirLights;

#define DIFFUSE_POWER 1
#define BRIGHTNESS_MULT 1
#define SPECULAR_FOCUS 32
#define SPECULAR_POWER 3

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec2 finalTexCoords;

void main() 
{
    // Final color output.
    vec4 color = subpassLoad(inputs[0]).rgba;
    vec4 position = subpassLoad(inputs[1]).rgba;
    vec4 normal = subpassLoad(inputs[2]).xyzw;
	vec3 emission = subpassLoad(inputs[3]).rgb;

    vec3 lighting = vec3(0.3f) + emission; // Ambient component plus emissive colors.

    for(int i = 0; i < dirLights.globalData.count; ++i) 
    {
        // Get direction and color of the current light.
        vec4 lightDir = dirLights.data[i].direction;
        vec4 lightColor = dirLights.data[i].color;

        // Get camera view direction and reflect the light direction upon the normal.
        vec3 viewDir = normalize(mvp.viewPos.xyz - position.xyz);
        vec3 lightReflected = reflect(lightDir.xyz, normal.xyz);
        
        // Calculate light intensity.
        float normalDotLight = max(-dot(lightDir, normal), 0.0f);

        // Calculate specular term.
        float specTerm = max(dot(lightReflected, viewDir), 0.0f);

        // Add to final lighting.
        vec3 diffuse = normalDotLight * lightColor.rgb * DIFFUSE_POWER; // Lighting component.
        vec3 spec = pow(specTerm, SPECULAR_FOCUS) * SPECULAR_POWER * lightColor.rgb * normalDotLight;

		lighting += (diffuse + spec) * BRIGHTNESS_MULT;
    }

    outColor = vec4(color.rgb * lighting, 1.0f); // Multiply color by lighting level as output.
}

