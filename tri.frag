#version 450
#pragma shader_stage(fragment)
layout (set = 0, binding = 2) uniform sampler2D tex;
layout (location = 0) in vec2 texcoord;
layout (location = 1) in vec3 norm;
layout (location = 2) in vec3 fromEye;
layout (location = 0) out vec4 out_fragColor;
in vec4 gl_FragCoord;

void main() {
    //out_fragColor = texture(tex, reflect(normalize(fromEye), normalize(norm)));
    vec4 head_light = dot(normalize(-fromEye), normalize(norm)) * vec4(1,1,1,1);
    out_fragColor = head_light * texture(tex, texcoord);
    //out_fragColor = vec4(1,1,0,1);
}
