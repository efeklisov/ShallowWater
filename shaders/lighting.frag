#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 1, binding = 0) uniform sampler2D texSampler;

layout(push_constant) uniform PushConsts {
    vec4 clipPlane;
    vec3 lightSource;
    vec3 lightColor;
} pushConsts;


layout(location = 0) in vec3 fragNormals;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragPos;
layout(location = 3) in vec3 fragCameraPos;

layout(location = 0) out vec4 outColor;

void main() {
    // ambient
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * pushConsts.lightColor;
  	
    // diffuse 
    vec3 norm = normalize(fragNormals);
    vec3 lightDir = normalize(pushConsts.lightSource - fragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * pushConsts.lightColor;
    
    // specular
    float specularStrength = 0.1;
    vec3 viewDir = normalize(fragCameraPos - fragPos);
    vec3 reflectDir = reflect(-lightDir, norm);  
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 16);
    vec3 specular = specularStrength * spec * pushConsts.lightColor;  
        
    vec3 result = (ambient + diffuse + specular) * texture(texSampler, fragTexCoord).xyz;
    outColor = vec4(result, 1.0);
}
