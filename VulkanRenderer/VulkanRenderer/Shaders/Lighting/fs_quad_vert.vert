#version 450
#extension GL_ARB_separate_shader_objects : enable

// Since we are drawing a simple quad storing the vertices here will suffice.
vec2 positions[6] = vec2[]
(
    vec2(-1.0f, -1.0f),
    vec2(1.0f, -1.0f),
    vec2(-1.0f, 1.0f),
    vec2(1.0f, -1.0f),
    vec2(1.0f, 1.0f),
    vec2(-1.0f, 1.0f)
);

layout(location = 0) out vec2 finalTexCoords;

void main() 
{
    vec2 position = positions[gl_VertexIndex];
    
    // Get texture coordinates from vertex position.
    finalTexCoords = (position + vec2(1.0f)) * 0.5f;

    gl_Position = vec4(position, 0.0f, 1.0f);
}