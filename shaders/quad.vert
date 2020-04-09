#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 invertView;
    mat4 invertModel;
    vec3 cameraPos;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormals;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec4 clipSpace;
layout(location = 1) out vec3 toCamera;

void main() {
    vec4 worldPosition = ubo.model * vec4(inPosition, 1.0);
    clipSpace = ubo.proj * ubo.view * worldPosition;

    gl_Position = clipSpace;
    toCamera = ubo.cameraPos - worldPosition.xyz;
}
