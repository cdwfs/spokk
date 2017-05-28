#version 450
#pragma shader_stage(fragment)
layout (set = 0, binding = 2) uniform texture2D tex;
layout (set = 0, binding = 3) uniform sampler samp;
layout (location = 0) in vec2 texcoord;
layout (location = 1) in vec3 norm;
layout (location = 2) in vec3 fromEye;
layout (location = 0) out vec4 out_fragColor;
in vec4 gl_FragCoord;

#include "cookbook.glsl"

void main() {
    Material mat;
    mat.normal_wsn = norm;
    mat.albedo = texture(sampler2D(tex,samp), texcoord);
    mat.spec_exp = 1.0;
    mat.spec_intensity = 0.0;

    HemiLight hemi;
    hemi.down_color = vec3(1,0,1);
    hemi.up_color = vec3(1,1,0);

    vec3 hemi_color = ApplyHemiLight(mat, hemi);
    out_fragColor.xyz = hemi_color* texture(sampler2D(tex, samp), texcoord).xyz;
    out_fragColor.w = 1;
}
