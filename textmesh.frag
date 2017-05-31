#version 450
#pragma shader_stage(fragment)
layout (set = 0, binding = 2) uniform texture2D atlas_tex;
layout (set = 0, binding = 3) uniform sampler samp;
layout (location = 0) in vec2 texcoord;
layout (location = 1) in vec3 tint;
layout (location = 0) out vec4 out_fragColor;
in vec4 gl_FragCoord;

void main() {
  float t = texture(sampler2D(atlas_tex,samp), texcoord).x + 0.05;
  out_fragColor = vec4(tint * t, t);
}
