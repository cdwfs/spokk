#version 450
#pragma shader_stage(fragment)
layout (location = 0) in vec3 pos_ws;
layout (location = 1) in vec3 norm_ws;
layout (location = 0) out vec4 out_fragColor;
in vec4 gl_FragCoord;

layout (set = 0, binding = 0) uniform SceneUniforms {
  vec4 time_and_res;  // x: elapsed seconds, yz: viewport resolution in pixels
  vec4 eye_pos_ws;    // xyz: world-space eye position
  vec4 eye_dir_wsn;   // xyz: world-space eye direction (normalized)
  // truncated; this should really be in a header file
} scene_consts;


#include <common/cookbook.glsl>

void main() {
  vec3 albedo = vec3(1,1,1);

  Material mat;
  mat.normal_wsn = normalize(norm_ws);
  mat.spec_exp = 1000.0;
  mat.spec_intensity = 1.0;

  HemiLight hemi_light;
  hemi_light.down_color = 0.5 * vec3(1,0,1);
  hemi_light.up_color = 0.5 * vec3(0,1,0);
  vec3 hemi_color = ApplyHemiLight(mat, hemi_light);

  DirLight dir_light;
  dir_light.to_light_wsn = vec3(1,0,0);
  dir_light.color = 0.5 * vec3(1,0,0);
  vec3 dir_color = ApplyDirLight(pos_ws, scene_consts.eye_pos_ws.xyz, mat, dir_light);

  PointLight point_light;
  point_light.pos_ws = vec3(0,0,-5);
  point_light.inverse_range = 0.0001f;
  point_light.color = 1.0 * vec3(0,0,1);
  vec3 point_color = ApplyPointLight(pos_ws, scene_consts.eye_pos_ws.xyz, mat, point_light);

  out_fragColor.xyz = (hemi_color + dir_color + point_color) * albedo;
  out_fragColor.w = 1;
}
