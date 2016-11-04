#version 450
layout (binding = 0) uniform sampler2D tex;
layout (location = 0) in vec2 texcoord;
layout (location = 1) in vec3 norm;
layout (location = 2) in vec3 fromEye;
layout (location = 0) out vec4 out_fragColor;
in vec4 gl_FragCoord;

layout (push_constant) uniform PushConsts {
    vec4 time_and_res;
    vec4 eye;
    mat4 viewproj;
} pushConsts;

vec4 film_grain() {
    // Adapted from https://www.shadertoy.com/view/4sXSWs
    vec2 iResolution = pushConsts.time_and_res.yz;
    float iGlobalTime = pushConsts.time_and_res.x;
    float strength = 4.0;

    vec2 uv = gl_FragCoord.xy / vec2(1280,720);
    float x = (uv.x + 4.0 ) * (uv.y + 4.0 ) * (iGlobalTime * 10.0);
	vec4 grain = vec4(mod((mod(x, 13.0) + 1.0) * (mod(x, 123.0) + 1.0), 0.01)-0.005) * strength;

    // By default, grain is additive. For multiplicative grain, uncomment this line:
    grain = 1.0 - grain;

    return grain;
}
void main() {
    //out_fragColor = texture(tex, reflect(normalize(fromEye), normalize(norm)));
    vec4 head_light = dot(normalize(-fromEye), normalize(norm)) * vec4(1,1,1,1);
    out_fragColor = head_light * texture(tex, texcoord) * film_grain();
    //out_fragColor = vec4(1,1,0,1);
}
