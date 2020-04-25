#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 invertView;
    mat4 invertModel;
    vec4 cameraPos;
} ubo;

layout(push_constant) uniform PushConsts {
    vec4 clipPlane;
    vec3 lightSource;
    vec3 lightColor;
    vec3 invert;
} pushConsts;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormals;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragNormals;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragPos;
layout(location = 3) out vec3 fragCameraPos;

void main() {
    vec4 worldPosition = ubo.model * vec4(inPosition, 1.0);
    gl_ClipDistance[0] = dot(worldPosition, pushConsts.clipPlane);

    if (pushConsts.invert.x > 0.5) {
        gl_Position = ubo.proj * ubo.invertView * worldPosition;
    } else gl_Position = ubo.proj * ubo.view * worldPosition;

    fragNormals = mat3(transpose(inverse(ubo.model))) * inNormals;
    fragTexCoord = inTexCoord;
    fragPos = worldPosition.xyz;
    fragCameraPos = ubo.cameraPos.xyz;
}
