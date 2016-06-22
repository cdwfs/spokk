#version 450
layout (binding = 0) uniform samplerCube tex;
layout (location = 0) in vec2 texcoord;
layout (location = 1) in vec3 norm;
layout (location = 2) in vec3 fromEye;
layout (location = 0) out vec4 uFragColor;
void main() {
    vec4 cube = texture(tex, reflect(normalize(fromEye), normalize(norm)));

    uFragColor = cube;
}
