#version 450
#pragma shader_stage(fragment)
layout (set = 0, binding = 1) uniform sampler2D atlas_tex;
layout (location = 0) in vec2 texcoord;
layout (location = 1) in vec3 tint;
layout (location = 0) out vec4 out_fragColor;
in vec4 gl_FragCoord;

void main() {
  float t = texture(atlas_tex, texcoord).x;
  out_fragColor = vec4(tint * t, t);
}
