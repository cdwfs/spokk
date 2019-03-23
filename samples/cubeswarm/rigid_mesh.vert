#version 450
#pragma shader_stage(vertex)

#include <spokk_shader_interface.h>

layout (location = SPOKK_VERTEX_ATTRIBUTE_LOCATION_POSITION) in vec3 pos;
layout (location = SPOKK_VERTEX_ATTRIBUTE_LOCATION_NORMAL) in vec3 normal;
layout (location = 0) out vec3 norm;
layout (location = 1) out vec3 fromEye;

layout (set = 0, binding = 0) uniform SceneUniforms {
  vec4 res_and_time;
  vec4 eye;
  mat4 viewproj;
} scene_consts;
layout (set = 0, binding = 1) uniform MeshUniforms {
  vec4 matrix_columns[4*1024];
} mesh_consts;


void main() {
  mat4 o2w = mat4(
    mesh_consts.matrix_columns[4*gl_InstanceIndex+0],
    mesh_consts.matrix_columns[4*gl_InstanceIndex+1],
    mesh_consts.matrix_columns[4*gl_InstanceIndex+2],
    mesh_consts.matrix_columns[4*gl_InstanceIndex+3]);

  mat3 n2w = mat3(o2w);
  norm = n2w * normal;
    
  vec4 posw = o2w * vec4(pos,1);
  fromEye = posw.xyz - scene_consts.eye.xyz;
  vec4 outpos = scene_consts.viewproj * posw;
  gl_Position = outpos;
}
