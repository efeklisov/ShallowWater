#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 1) uniform sampler2D refract;
layout(set = 1, binding = 0) uniform sampler2D reflect;

layout(location = 0) in vec4 clipSpace;
layout(location = 1) in vec3 inCamera;

layout(location = 0) out vec4 outColor;

void main() {
    vec2 ndc = clipSpace.xy / clipSpace.w / 2.0 + 0.5;
    vec4 refractFrag = texture(refract, ndc);
    ndc.y *= -1;
    vec4 reflectFrag = texture(reflect, ndc);

    float refractiveFactor = dot(normalize(inCamera), vec3(0.0, 1.0, 0.0));
    float reflectiveFactor = pow(refractiveFactor, 2.0);
    outColor = mix(refractFrag, reflectFrag, reflectiveFactor);
}
