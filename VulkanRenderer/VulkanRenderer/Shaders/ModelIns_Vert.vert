#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (set = 0, binding = 0) uniform UniformBuffer 
{
	mat4 view;
	mat4 proj;
	vec4 viewPos;
} mvp;

layout(location = 0) in vec4 position;
layout(location = 1) in vec4 normal;
layout(location = 2) in vec4 tangent;
layout(location = 3) in vec2 texCoords;
layout(location = 4) in mat4 model;

layout(location = 0) out vec4 f_finalPos;
layout(location = 1) out vec2 f_texCoords;
layout(location = 2) out mat3 f_tbn;

void main() 
{
    f_finalPos = model * position;
    //finalNormal = model * normal; // Transform mesh normal and pass to the next shader stage.
    f_texCoords = texCoords;

	vec4 biTangent = vec4(cross(normal.xyz, tangent.xyz), 1.0f);

	mat4 modelCpy = model;
	modelCpy[3] = vec4(0.0f, 0.0f, 0.0f, 1.0f);

	vec4 t = modelCpy * tangent;
	vec4 b = modelCpy * biTangent;
	vec4 n = modelCpy * normal;

	f_tbn = mat3(t.xyz, b.xyz, n.xyz);

    gl_Position = mvp.proj * mvp.view * model * position;
}