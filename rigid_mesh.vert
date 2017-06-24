#version 450
#pragma shader_stage(vertex)
layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 attr;
layout (location = 0) out vec3 pos_ws;
layout (location = 1) out vec3 norm_ws;
layout (location = 2) out vec2 texcoord;

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
  vec4 matrix_columns[4*1024];
} mesh_consts;


void main() {
  mat4 o2w = mat4(
    mesh_consts.matrix_columns[4*gl_InstanceIndex+0],
    mesh_consts.matrix_columns[4*gl_InstanceIndex+1],
    mesh_consts.matrix_columns[4*gl_InstanceIndex+2],
    mesh_consts.matrix_columns[4*gl_InstanceIndex+3]);

  mat3 n2w = mat3(o2w);
  vec4 posw = o2w * vec4(pos,1);

  pos_ws = posw.xyz;
  norm_ws = n2w * normal;
  texcoord = attr;
  gl_Position = scene_consts.viewproj * posw;
}
