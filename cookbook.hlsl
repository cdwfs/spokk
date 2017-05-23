struct Material {
    float3 normal_wsn;
    float4 albedo;
    float spec_exp;        // for matte, set to 1
    float spec_intensity;  // for matte, set to 0
};

////////////////////////////

struct HemiLight {
    float3 down_color;
    float3 up_color;
};

float3 ApplyHemiLight(float3 pos_ws, Material mat, HemiLight light) {
    float up = mat.normal_wsn.y * 0.5 + 0.5;  // TODO(cort): assumes hemisphere axis is world-space +Y
    float3 amb_color = lerp(light.down_color, light.up_color, up);
    // amb_color = light.down_color + light.up_minus_down_color * up;
    return amb_color * mat.albedo.rgb;
}

///////////////////////////

struct DirLight {
    float3 to_light_wsn;
    float3 color;
};

float3 ApplyDirLight(float3 pos_ws, float3 eye_pos_ws,
        Material mat, DirLight light) {
    float n_dot_l = dot(light.to_light_wsn, mat.normal_wsn);
    float3 dif_color = light.color * saturate(n_dot_l);

    float3 spec_color = float3(0,0,0);
    if (mat.spec_exp != 1.0) {
        float3 to_eye_wsn = normalize(eye_pos_ws - pos_ws);
        float3 halfway_wsn = normalize(to_eye_wsn + light.to_light_wsn);
        float n_dot_h = saturate(dot(halfway_wsn, mat.normal_wsn));
        spec_color += light.color.rgb * pow(n_dot_h, mat.spec_intensity);
    }
    return (dif_color + spec_color) * mat.albedo.rgb;
}

//////////////////////////

struct PointLight {
    float3 pos_ws;
    float inverse_range;
};

float3 ApplyPointLight(float3 pos_ws, float3 eye_pos_ws, Material mat, PointLight light) {
    float3 to_light_ws = light.pos_ws - pos_ws;
    float3 to_eye = eye_pos_ws - pos_ws;
}
