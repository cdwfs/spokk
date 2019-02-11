#version 450
#pragma shader_stage(vertex)
layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 attr;
layout (location = 0) out vec2 texcoord;
layout (location = 1) out vec3 norm;
layout (location = 2) out vec3 fromEye;

layout (set = 0, binding = 0) uniform SceneUniforms {
  vec4 res_and_time;
  vec4 eye;
  mat4 viewproj;
} scene_consts;
layout (set = 0, binding = 1) uniform isamplerBuffer visible_cells;
layout (set = 0, binding = 2) uniform samplerBuffer cell_heights;

void main() {
  int cell = texelFetch(visible_cells, gl_InstanceIndex).x;
  int cell_x = cell % 256;
  int cell_z = cell / 256;

  texcoord = attr;

  norm = normal;

  vec3 vpos = 0.5 * pos + vec3(0.5, 0.5, 0.5);
  vpos.y *= texelFetch(cell_heights, cell).x;
  vec4 posw = vec4(vpos + vec3(cell_x, 0, cell_z), 1);
  fromEye = posw.xyz - scene_consts.eye.xyz;
  vec4 outpos = scene_consts.viewproj * posw;
  gl_Position = outpos;
}
