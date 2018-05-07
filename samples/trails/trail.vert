#version 450
#extension GL_ARB_shader_draw_parameters : require
#pragma shader_stage(vertex)


#include <spokk_shader_interface.h>

layout (location = 0) out vec3 fromEye;

layout (set = 0, binding = 0) uniform SceneUniforms {
  vec4 time_and_res;
  vec4 eye;
  mat4 viewproj;
  ivec4 trail_params;
} scene_consts;
layout (set = 0, binding = 1) uniform samplerBuffer trail_positions;
layout (set = 0, binding = 2) uniform usamplerBuffer trail_lengths;
layout (set = 0, binding = 3) uniform usamplerBuffer trail_age_offsets;

void main() {
  const int particle_index = gl_BaseInstanceARB;
  const int points_per_trail = scene_consts.trail_params.x;
  const int index = (gl_VertexIndex & (points_per_trail - 1)) +
    particle_index * points_per_trail;
  vec4 posw = texelFetch(trail_positions, index);
  fromEye = posw.xyz - scene_consts.eye.xyz;
  vec4 outpos = scene_consts.viewproj * posw;
  gl_Position = outpos;
}
