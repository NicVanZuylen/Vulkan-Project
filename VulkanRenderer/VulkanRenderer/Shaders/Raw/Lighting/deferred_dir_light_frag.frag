#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput inputs[3];

vec4 lightDir = vec4(0.0f, 0.0f, -1.0f, 1.0f);
vec4 lightColor = vec4(1.0f);

#define BRIGHTNESS_MULT 3

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec2 finalTexCoords;

void main() 
{
    // Final color output.
    vec4 color = subpassLoad(inputs[0]).rgba;
    vec4 normal = subpassLoad(inputs[2]).xyzw;

    float normalDotLight = max(-dot(lightDir, normal), 0.0f);

    vec3 lighting = vec3(0.3f); // Ambient component.
    lighting += normalDotLight * lightColor.rgb * BRIGHTNESS_MULT; // Lighting component.

    outColor = vec4(color.rgb * lighting, 1.0f); // Multiply color by lighting level as output.
}

