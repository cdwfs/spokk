#version 450
#pragma shader_stage(vertex)

#include <spokk_shader_interface.h>

layout (location = SPOKK_VERTEX_ATTRIBUTE_LOCATION_POSITION) in vec3 pos;
layout (location = SPOKK_VERTEX_ATTRIBUTE_LOCATION_NORMAL) in vec3 normal;
layout (location = 0) out vec3 pos_ws;
layout (location = 1) out vec3 norm_ws;

layout (set = 0, binding = 0) uniform SceneUniforms {
  vec4 res_and_time;
  vec4 eye;
  mat4 viewproj;
} scene_consts;
layout (set = 0, binding = 1) uniform MeshUniforms {
  mat4 o2w;
  vec4 albedo; // xyz=color, w=opacity
  vec4 spec_params;  // x=exponent, y=intensity
} mesh_consts;


void main() {
  mat3 n2w = mat3(mesh_consts.o2w);
  vec4 posw = mesh_consts.o2w * vec4(pos,1);

  pos_ws = posw.xyz;
  norm_ws = n2w * normal;
  gl_Position = scene_consts.viewproj * posw;
}
