#version 450
#pragma shader_stage(fragment)
layout (location = 0) in vec3 pos_ws;
layout (location = 1) in vec3 norm_ws;
// dual-source blending!
layout (location = 0, index = 0) out vec4 out_fragColorA;
layout (location = 0, index = 1) out vec4 out_fragColorB;

layout (set = 0, binding = 0) uniform SceneUniforms {
  vec4 time_and_res;
  vec4 eye;
  mat4 viewproj;
} scene_consts;

struct Material {
  vec3 albedo_color;
  vec3 normal_wsn;
  vec3 spec_color;  // 1.0 for most reflective objects; match albedo color for metals
  float spec_exp;  // for matte, set to 1
  float spec_intensity;  // for matte, set to 0
};

struct PointLight {
  vec3 pos_ws;
  float inverse_range;
  vec3 color;
};

void main() {
    Material mat;
    mat.albedo_color = vec3(1.0, 0.5, 0.2);  // demi-orange
    mat.normal_wsn = normalize(norm_ws);
    mat.spec_color = vec3(1,1,1);
    mat.spec_intensity = 1.0;
    mat.spec_exp = 100.0;

    PointLight light;
    light.pos_ws = vec3(0.0, 0.0, 5.0);
    light.inverse_range = 1.0 / 100.0;
    light.color = vec3(1,1,1);

    // apply light
    vec3 to_light = light.pos_ws - pos_ws;
    float to_light_len = length(to_light);
    vec3 to_light_wsn = to_light / to_light_len;
    float n_dot_l = dot(to_light_wsn, mat.normal_wsn);
    float attenuation = 1.0 - clamp(to_light_len * light.inverse_range, 0, 1);

    vec3 dif_color = light.color * clamp(n_dot_l, 0, 1);

    vec3 spec_color = vec3(0, 0, 0);
    if (mat.spec_exp != 1.0) {
      vec3 to_eye_wsn = normalize(scene_consts.eye.xyz - pos_ws);
      vec3 halfway_wsn = normalize(to_eye_wsn + to_light_wsn);
      float n_dot_h = clamp(dot(halfway_wsn, mat.normal_wsn), 0, 1);
      spec_color += light.color.rgb * pow(n_dot_h, mat.spec_exp) * mat.spec_intensity;
    }

    // This color will be applied additively
    out_fragColorA = vec4(spec_color * mat.spec_color * attenuation, 1.0);
    // This color will be multiplied by the existing framebuffer color
    out_fragColorB = vec4(dif_color * mat.albedo_color * attenuation, 1.0);
}
