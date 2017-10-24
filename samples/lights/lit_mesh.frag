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
layout (set = 0, binding = 2) uniform LightUniforms {
  vec4 hemi_down_color;  // xyz: RGB color, w: intensity
  vec4 hemi_up_color;  // xyz: RGB color

  vec4 dir_color;  // xyz: RGB color, w: intensity
  vec4 dir_to_light_wsn; // xyz: world-space normalized vector towards light

  vec4 point_pos_ws_inverse_range; // xyz: world-space light pos, w: inverse range of light
  vec4 point_color;  // xyz: RGB color, w: intensity
} light_consts;


#include <common/cookbook.glsl>

void main() {
  vec3 albedo = vec3(1,1,1);

  Material mat;
  mat.normal_wsn = normalize(norm_ws);
  mat.spec_exp = 1000.0;
  mat.spec_intensity = 1.0;

  HemiLight hemi_light;
  hemi_light.down_color = light_consts.hemi_down_color.xyz;
  hemi_light.up_color = light_consts.hemi_up_color.xyz;
  vec3 hemi_color = light_consts.hemi_down_color.w * ApplyHemiLight(mat, hemi_light);

  DirLight dir_light;
  dir_light.to_light_wsn = light_consts.dir_to_light_wsn.xyz;
  dir_light.color = light_consts.dir_color.xyz;
  vec3 dir_color = light_consts.dir_color.w * ApplyDirLight(pos_ws, scene_consts.eye_pos_ws.xyz, mat, dir_light);

  PointLight point_light;
  point_light.pos_ws = light_consts.point_pos_ws_inverse_range.xyz;
  point_light.inverse_range = light_consts.point_pos_ws_inverse_range.w;
  point_light.color = light_consts.point_color.xyz;
  vec3 point_color = light_consts.point_color.w * ApplyPointLight(pos_ws, scene_consts.eye_pos_ws.xyz, mat, point_light);

  out_fragColor.xyz = (hemi_color + dir_color + point_color) * albedo;
  out_fragColor.w = 1;
}
