#version 450
#pragma shader_stage(fragment)
layout (location = 0) in vec2 texcoord;
layout (location = 1) in vec3 norm;
layout (location = 2) in vec3 fromEye;
layout (location = 0) out vec4 out_fragColor;
in vec4 gl_FragCoord;

layout (set = 0, binding = 3) uniform texture2D tex;
layout (set = 0, binding = 4) uniform sampler samp;


void main() {
    vec4 head_light_color = vec4(1,1,1,1);
    vec4 head_light = dot(normalize(-fromEye), normalize(norm)) * head_light_color;
    out_fragColor = head_light * texture(sampler2D(tex, samp), texcoord);
}
