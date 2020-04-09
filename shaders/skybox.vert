#version 450

layout (location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormals;
layout(location = 2) in vec2 inTexCoord;

layout(push_constant) uniform PushConsts {
    vec4 clipPlane;
    vec3 lightSource;
    vec3 lightColor;
    float invert;
} pushConsts;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 invertView;
    mat4 invertModel;
    vec3 cameraPos;
} ubo;

layout (location = 0) out vec3 outUVW;

void main() 
{
	outUVW = inPos;
	outUVW.x *= -1.0;

    if (pushConsts.invert.x > 0.5) {
        gl_Position = ubo.proj * ubo.invertView * ubo.invertModel * vec4(inPos.xyz, 1.0);
    } else gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPos.xyz, 1.0);
}
