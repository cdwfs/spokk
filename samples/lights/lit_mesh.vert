#version 450
#pragma shader_stage(vertex)

#include <spokk_shader_interface.h>

layout (location = SPOKK_VERTEX_ATTRIBUTE_LOCATION_POSITION) in vec3 pos_os;
layout (location = SPOKK_VERTEX_ATTRIBUTE_LOCATION_NORMAL) in vec3 norm_os;
layout (location = SPOKK_VERTEX_ATTRIBUTE_LOCATION_TEXCOORD0) in vec2 texcoord;
layout (location = 0) out vec3 pos_ws;
layout (location = 1) out vec3 norm_ws;
layout (location = 2) out vec2 uv;

layout (set = 0, binding = 0) uniform SceneUniforms {
  vec4 time_and_res;  // x: elapsed seconds, yz: viewport resolution in pixels
  vec4 eye_pos_ws;    // xyz: world-space eye position
  vec4 eye_dir_wsn;   // xyz: world-space eye direction (normalized)
  mat4 viewproj;
  mat4 view;
  mat4 proj;
  mat4 viewproj_inv;
  mat4 view_inv;
  mat4 proj_inv;
} scene_consts;
layout (set = 0, binding = 1) uniform MeshUniforms {
  mat4 o2w;
} mesh_consts;


void main() {
  mat3 n2w = mat3(mesh_consts.o2w);
  vec4 posw = mesh_consts.o2w * vec4(pos_os,1);

  pos_ws = posw.xyz;
  norm_ws = n2w * norm_os;
  uv = texcoord;
  gl_Position = scene_consts.viewproj * posw;
}
