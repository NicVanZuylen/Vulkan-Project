#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (set = 0, binding = 0) uniform UniformBuffer 
{
	mat4 view;
	mat4 proj;
	mat4 invView;
	mat4 invProj;
	vec4 viewPos;
	vec2 framebufferDimensions;
	float nearPlane;
	float farPlane;
} mvp;

layout(input_attachment_index = 0, set = 1, binding = 0) uniform subpassInput inputs[5];

layout(set = 3, binding = 0) uniform ShadowMapCamera
{
    mat4 lightSpaceMatrix;
} shadowMapCamera;

//layout(set = 3, binding = 1) uniform sampler2D shadowMap;

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
#define SPECULAR_POWER 0.5f

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec2 finalTexCoords;

float OrenNayarDiff(vec3 normal, vec3 lightDir, vec3 surfToCam, float roughness) 
{
    float roughSqr = roughness * roughness;

    float A = 1.0f - (0.5f * (roughSqr / (roughSqr + 0.33f)));
	float B = 0.45f * (roughSqr / (roughSqr + 0.09f));

	float normalDotLight = max(dot(normal, lightDir), 0.0f);
	float normalDotSurfToCam = max(dot(normal, surfToCam), 0.0f);

	vec3 lightProj = normalize(lightDir - (normal * normalDotLight));
	vec3 viewProj = normalize(surfToCam - (normal * normalDotSurfToCam));

	float cx = max(dot(lightProj, viewProj), 0.0f);

	float alpha =  sin(max(acos(normalDotSurfToCam), acos(normalDotLight)));
	float beta = tan(min(acos(normalDotSurfToCam), acos(normalDotLight)));

	float dx = alpha * beta;

	return normalDotLight * (A + B * cx * dx);
}

#define PI 3.14159265359f

float CookTorrenceSpec(vec3 normal, vec3 lightDir, vec3 viewDir, float lambert, float roughness, float relectionCoefficient) 
{
    float roughSqr = roughness * roughness;

	float normalDotView = max(dot(normal, viewDir), 0.0f);

	vec3 halfVec = normalize(lightDir + viewDir);

	float normalDotHalf = max(dot(normal, halfVec), 0.0f);
	float normalDotHalfSqr = normalDotHalf * normalDotHalf;

	// Beckmann Distribution
	float exponent = -(1.0f - normalDotHalfSqr) / (normalDotHalfSqr * roughSqr);
	float D = exp(exponent) / (roughSqr * normalDotHalfSqr * normalDotHalfSqr);

	// Fresnel Term using Sclick's approximation.
	float F = relectionCoefficient + (1.0f - relectionCoefficient) * pow(1.0f - normalDotView, 5);

	// Geometric Attenuation Factor
	float halfFrac = 2.0f * normalDotHalf / dot(viewDir, halfVec);
	float G = min(1.0f, min(halfFrac * normalDotView, halfFrac * lambert));

	float bottomHalf = PI * normalDotView;

	return max((D * F * G) / bottomHalf, 0.0f);
}

vec3 WorldPosFromDepth(float depth, vec2 texCoords) 
{
	// Convert x & y to clip space and include z.
	vec4 clipSpacePos = vec4(texCoords * 2.0f - 1.0f, depth, 1.0f);
	vec4 viewSpacepos = mvp.invProj * clipSpacePos; // Transform clip space position to view space.

	// Do perspective divide
	viewSpacepos /= viewSpacepos.w;

	// Transform to worldspace from viewspace.
	vec4 worldPos = mvp.invView * viewSpacepos;

	return worldPos.xyz;
}

void main() 
{
    // Final color output.
    vec4 color = subpassLoad(inputs[0]).rgba;
    vec4 normal = subpassLoad(inputs[1]);
	vec3 emission = subpassLoad(inputs[2]).rgb;
	
	vec4 specRoughness = subpassLoad(inputs[3]);
	float roughness = specRoughness.a;

	float depth = subpassLoad(inputs[4]).r;

    vec3 lighting = vec3(0.3f) + emission; // Ambient component plus emissive colors.

	// Calculate world space position from depth.
	vec3 position = WorldPosFromDepth(depth, finalTexCoords);

    for(int i = 0; i < dirLights.globalData.count; ++i) 
    {
        // Get direction and color of the current light.
        vec4 lightDir = dirLights.data[i].direction;
        vec4 lightColor = dirLights.data[i].color;

        // Get camera view direction and reflect the light direction upon the normal.
        vec3 viewDir = normalize(mvp.viewPos.xyz - position);
        
        // Calculate lambertian term.
        float lambert = max(-dot(lightDir, normal), 0.0f);

		// Calculate Oren Nayar Diffuse & Cook Torrence Specular values.
		float orenNayar = OrenNayarDiff(normal.xyz, -lightDir.xyz, viewDir, roughness);
		float cookTorrence = CookTorrenceSpec(normal.xyz, -lightDir.xyz, viewDir, lambert, roughness, 1.0f);

        // Add to final lighting.
		vec3 diffuse = orenNayar * lightColor.rgb * DIFFUSE_POWER;
		vec3 spec = cookTorrence * SPECULAR_POWER * lightColor.rgb * lambert;

		lighting += (diffuse + spec) * BRIGHTNESS_MULT;
    }

    outColor = vec4(color.rgb * lighting, 1.0f); // Multiply color by lighting level as output.
}

