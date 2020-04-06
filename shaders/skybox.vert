#version 450

layout (location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormals;
layout(location = 2) in vec2 inTexCoord;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 cameraPos;
} ubo;

layout (location = 0) out vec3 outUVW;

void main() 
{
	outUVW = inPos;
	outUVW.x *= -1.0;
	gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPos.xyz, 1.0);
}
