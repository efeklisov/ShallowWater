#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 1) uniform sampler2D refract;
layout(set = 1, binding = 0) uniform sampler2D reflect;

layout(location = 0) in vec4 beforeDistortion;
layout(location = 1) in vec3 inCamera;
layout(location = 2) in vec3 normal;

layout(location = 0) out vec4 outColor;

vec2 projecticeTexturing(vec4 point) {
    return clamp(point.xy / point.w / 2.0 + 0.5, 0.01, 0.999);
}

void main() {
    vec2 before = projecticeTexturing(beforeDistortion);
    
    vec4 refractFrag = texture(refract, before);
    before.y *= -1;
    vec4 reflectFrag = texture(reflect, before);

    float refractiveFactor = clamp(dot(normalize(inCamera), normal), 0.0, 1.0);
    float reflectiveFactor = clamp(pow(refractiveFactor, 2.0), 0.0, 1.0);
    outColor = mix(refractFrag, reflectFrag, reflectiveFactor);
}
