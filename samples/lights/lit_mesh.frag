#version 450
#pragma shader_stage(fragment)
layout (location = 0) in vec3 pos_ws;
layout (location = 1) in vec3 norm_ws;
layout (location = 0) out vec4 out_fragColor;
in vec4 gl_FragCoord;

layout (set = 0, binding = 0) uniform SceneUniforms {
  vec4 res_and_time;  // xy: viewport resolution in pixels, z: unused, w: elapsed seconds
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

  vec4 spot_pos_ws_inverse_range;  // xyz: world-space light pos, w: inverse range of light
  vec4 spot_color;  // xyz: RGB color, w: intensity
  vec4 spot_neg_dir_wsn;  // xyz: world-space normalized light direction (negated)
  vec4 spot_falloff_angles;  // x: 1/(cos(inner)-cos(outer)), y: cos(outer)
} light_consts;

layout (set = 0, binding = 3) uniform MaterialUniforms {
  vec4 albedo;  // xyz: albedo RGB
  vec4 emissive_color;  // xyz: emissive color, w: intensity
  vec4 spec_color;  // xyz: specular RGB, w: intensity
  vec4 spec_exp;  // x: specular exponent
} mat_consts;


#include <common/cookbook.glsl>

void main() {
  Material mat;
  mat.albedo_color = mat_consts.albedo.xyz;
  mat.normal_wsn = normalize(norm_ws);
  mat.spec_color = mat_consts.spec_color.xyz;
  mat.spec_intensity = mat_consts.spec_color.w;
  mat.spec_exp = mat_consts.spec_exp.x;

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

  SpotLight spot_light;
  spot_light.pos_ws = light_consts.spot_pos_ws_inverse_range.xyz;
  spot_light.inverse_range = light_consts.spot_pos_ws_inverse_range.w;
  spot_light.color = light_consts.spot_color.xyz;
  spot_light.neg_light_dir_wsn = light_consts.spot_neg_dir_wsn.xyz;
  spot_light.inv_inner_outer = light_consts.spot_falloff_angles.x;
  spot_light.cosine_outer = light_consts.spot_falloff_angles.y;
  vec3 spot_color = light_consts.spot_color.w * ApplySpotLight(pos_ws, scene_consts.eye_pos_ws.xyz, mat, spot_light);

  vec3 emissive_color = mat_consts.emissive_color.xyz * mat_consts.emissive_color.w;

  out_fragColor.xyz = hemi_color + dir_color + point_color + spot_color + emissive_color;
  out_fragColor.w = 1;
}
