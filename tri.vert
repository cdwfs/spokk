#version 450
layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 attr;
layout (location = 0) out vec2 texcoord;
layout (location = 1) out vec3 norm;
layout (location = 2) out vec3 fromEye;

layout (set = 0, binding = 1) uniform ObjectToWorld {
    mat4 o2w_matrices[1024];
};

layout (push_constant) uniform PushConsts {
    vec4 time;
    vec4 eye;
    mat4 viewproj;
} pushConsts;

void main() {
    texcoord = attr;

    mat4 o2w = o2w_matrices[gl_InstanceIndex];

    mat3 n2w = mat3(o2w);
    norm = n2w * normal;
    
    vec4 posw = o2w * vec4(pos,1);
    fromEye = posw.xyz - pushConsts.eye.xyz;
    vec4 outpos = pushConsts.viewproj * posw;
    gl_Position = outpos;

}
