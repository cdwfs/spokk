#version 450
#pragma shader_stage(fragment)
layout (input_attachment_index = 0, set = 0, binding = 3) uniform subpassInput subpass_in;
layout (location = 0) out vec4 out_fragColor;
in vec4 gl_FragCoord;

layout (set = 0, binding = 0) uniform SceneUniforms {
  vec4 time_and_res;
  vec4 eye;
  mat4 viewproj;
};

vec4 film_grain() {
    // Adapted from https://www.shadertoy.com/view/4sXSWs
    vec2 iResolution = time_and_res.yz;
    float iGlobalTime = time_and_res.x;
    float strength = 16.0;

    vec2 uv = gl_FragCoord.xy / time_and_res.yz;
    float x = (uv.x + 4.0 ) * (uv.y + 4.0 ) * (iGlobalTime * 10.0);
	vec4 grain = vec4(mod((mod(x, 13.0) + 1.0) * (mod(x, 123.0) + 1.0), 0.01)-0.005) * strength;

    // By default, grain is additive. For multiplicative grain, uncomment this line:
    grain = 1.0 - grain;

    return grain;
}

void main() {
    out_fragColor = subpassLoad(subpass_in) * film_grain();
}
