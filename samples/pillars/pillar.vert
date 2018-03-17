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
} scene_consts;
layout (set = 0, binding = 1) uniform isamplerBuffer visible_cells;
layout (set = 0, binding = 2) uniform samplerBuffer cell_heights;

#define SIZ_ (256*1024*1024)
layout(set=0,binding=7,std430)                    buffer ssbo0_ {uint  bufA32[SIZ_/4];};
layout(set=0,binding=7,std430)                    buffer ssbo1_ {uvec2 bufA64[SIZ_/8];};
layout(set=0,binding=7,std430)           readonly buffer ssbo2_ {uint  bufR32[SIZ_/4];};
layout(set=0,binding=7,std430)           readonly buffer ssbo3_ {uvec2 bufR64[SIZ_/8];};
layout(set=0,binding=7,std430)           readonly buffer ssbo4_ {uvec4 bufR128[SIZ_/16];};
layout(set=0,binding=7,std430) coherent writeonly buffer ssbo5_ {uint  bufW32[SIZ_/4];};
layout(set=0,binding=7,std430) coherent writeonly buffer ssbo6_ {uvec2 bufW64[SIZ_/8];};
layout(set=0,binding=7,std430) coherent writeonly buffer ssbo7_ {uvec4 bufW128[SIZ_/16];};
layout(set=0,binding=7,std430) volatile writeonly buffer ssbo8_ {uint  bufV32[SIZ_/4];};
layout(set=0,binding=7,std430) volatile writeonly buffer ssbo9_ {uvec2 bufV64[SIZ_/8];};
layout(set=0,binding=7,std430) volatile writeonly buffer ssboA_ {uvec4 bufV128[SIZ_/16];};

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
