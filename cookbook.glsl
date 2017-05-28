struct Material {
    vec3 normal_wsn;
    vec4 albedo;
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
    return amb_color * mat.albedo.rgb;
}

///////////////////////////

