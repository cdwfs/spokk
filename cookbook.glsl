struct Material {
    vec3 normal_wsn;
    float spec_exp;        // for matte, set to 1
    float spec_intensity;  // for matte, set to 0
};

////////////////////////////

struct HemiLight {
    vec3 down_color;
	vec3 up_color;
    //vec3 up_minus_down_color;
};

vec3 ApplyHemiLight(Material mat, HemiLight light) {
    float up = mat.normal_wsn.y * 0.5 + 0.5;  // TODO(cort): assumes hemisphere up axis is world-space +Y
    vec3 amb_color = mix(light.down_color, light.up_color, up);
    //vec3 amb_color = light.down_color + light.up_minus_down_color * up;
    return amb_color;
}

///////////////////////////

struct DirLight {
    vec3 to_light_wsn;
    vec3 color;
};

vec3 ApplyDirLight(vec3 pos_ws, vec3 eye_pos_ws, Material mat, DirLight light) {
    float n_dot_l = dot(light.to_light_wsn, mat.normal_wsn);
    vec3 dif_color = light.color * clamp(n_dot_l, 0.0, 1.0);

    vec3 spec_color = vec3(0,0,0);
    if (mat.spec_exp != 1.0) {
        vec3 to_eye_wsn = normalize(eye_pos_ws - pos_ws);
        vec3 halfway_wsn = normalize(to_eye_wsn + light.to_light_wsn);
        float n_dot_h = clamp(dot(halfway_wsn, mat.normal_wsn), 0.0, 1.0);
        spec_color += light.color.rgb * pow(n_dot_h, mat.spec_exp) * mat.spec_intensity;
    }
    return (dif_color + spec_color);
}

////////////////////////

struct PointLight {
    vec3 pos_ws;
    float inverse_range;
    vec3 color;
};

vec3 ApplyPointLight(vec3 pos_ws, vec3 eye_pos_ws, Material mat, PointLight light) {
    vec3 to_light = light.pos_ws - pos_ws;
    float to_light_len = length(to_light);
    vec3 to_light_wsn = to_light / to_light_len;
    float n_dot_l = dot(to_light_wsn, mat.normal_wsn);
    vec3 dif_color = light.color * clamp(n_dot_l, 0, 1);

    vec3 spec_color = vec3(0,0,0);
    if (mat.spec_exp != 1.0) {
      vec3 to_eye_wsn = normalize(eye_pos_ws - pos_ws);
      vec3 halfway_wsn = normalize(to_eye_wsn + to_light_wsn);
      float n_dot_h = clamp(dot(halfway_wsn, mat.normal_wsn), 0, 1);
      spec_color += light.color.rgb * pow(n_dot_h, mat.spec_exp) * mat.spec_intensity;
    }

    float attenuation = 1.0 - clamp(to_light_len * light.inverse_range, 0, 1);
    attenuation *= attenuation;

    return (dif_color + spec_color) * attenuation;
}
