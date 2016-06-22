#version 450
layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 attr;
layout (location = 0) out vec2 texcoord;
layout (location = 1) out vec3 norm;
layout (location = 2) out vec3 fromEye;

layout (push_constant) uniform PushConsts {
    vec4 time;
    vec4 eye;
    mat4 o2w;
    mat4 viewproj;
    mat4 n2w;
} pushConsts;

void main() {
    texcoord = attr;

    // TODO(cort): figure out the appropriate packing/layout qualifier to pass a mathfu mat3
    // directly into GLSL, eliminating the need for this mat4-to-mat3 typecast.
    mat3 n2w = mat3(pushConsts.n2w);
    norm = n2w * normal;
    
    vec4 posw = pushConsts.o2w * vec4(pos,1);
    fromEye = posw.xyz - pushConsts.eye.xyz;
    vec4 outpos = pushConsts.viewproj * posw;
    gl_Position = outpos;

}
