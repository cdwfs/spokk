#version 450
#pragma shader_stage(vertex)

#include <spokk_shader_interface.h>

layout (location = SPOKK_VERTEX_ATTRIBUTE_LOCATION_POSITION) in vec3 pos;
layout (location = SPOKK_VERTEX_ATTRIBUTE_LOCATION_NORMAL) in vec3 normal;
layout (location = 0) out vec3 norm;
layout (location = 1) out vec3 fromEye;

layout (set = 0, binding = 0) uniform SceneUniforms {
  vec4 time_and_res;
  vec4 eye;
  mat4 viewproj;
} scene_consts;
layout (set = 0, binding = 1) uniform MeshUniforms {
  mat4 o2w;
} opaque_mesh_consts;


void main() {
  mat3 n2w = mat3(opaque_mesh_consts.o2w);
  norm = n2w * normal;
    
  vec4 posw = opaque_mesh_consts.o2w * vec4(pos,1);
  fromEye = posw.xyz - scene_consts.eye.xyz;
  vec4 outpos = scene_consts.viewproj * posw;
  gl_Position = outpos;
}
