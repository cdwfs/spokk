#version 450
#pragma shader_stage(vertex)
layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 attr;
layout (location = 0) out vec2 texcoord;
layout (location = 1) out vec3 norm;
layout (location = 2) out vec3 fromEye;

layout (set = 0, binding = 0) uniform SceneUniforms {
    vec4 time_and_res;
    vec4 eye;
    mat4 viewproj;
};
layout (set = 0, binding = 1) uniform ObjectToWorld {
    vec4 matrix_columns[4*1024];
};


void main() {
    texcoord = attr;

    mat4 o2w = mat4(
        matrix_columns[4*gl_InstanceIndex+0],
        matrix_columns[4*gl_InstanceIndex+1],
        matrix_columns[4*gl_InstanceIndex+2],
        matrix_columns[4*gl_InstanceIndex+3]);

    mat3 n2w = mat3(o2w);
    norm = n2w * normal;
    
    vec4 posw = o2w * vec4(pos,1);
    fromEye = posw.xyz - eye.xyz;
    vec4 outpos = viewproj * posw;
    gl_Position = outpos;

}
