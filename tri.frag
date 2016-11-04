#version 450
layout (binding = 0) uniform sampler2D tex;
layout (location = 0) in vec2 texcoord;
layout (location = 1) in vec3 norm;
layout (location = 2) in vec3 fromEye;
layout (location = 0) out vec4 out_fragColor;
void main() {
    //out_fragColor = texture(tex, reflect(normalize(fromEye), normalize(norm)));
    out_fragColor = texture(tex, texcoord);
    //out_fragColor = vec4(1,1,0,1);
}
