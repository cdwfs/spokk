#version 450
#pragma shader_stage(fragment)
layout (location = 0) in vec3 norm;
layout (location = 1) in vec3 fromEye;
layout (location = 0) out vec4 out_fragColor;
in vec4 gl_FragCoord;

void main() {
    vec3 head_light_color = vec3(1,1,1);
    vec3 head_light = dot(normalize(-fromEye), normalize(norm)) * head_light_color;
    out_fragColor = vec4(head_light, 1.0);
}
