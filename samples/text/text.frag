#version 450
#pragma shader_stage(fragment)
layout (set = 0, binding = 0) uniform StringUniforms {
  vec4 color;
  vec4 viewport_to_clip;
} string_consts;
layout (set = 1, binding = 0) uniform texture2D atlas_tex;
layout (set = 1, binding = 1) uniform sampler samp;
layout (location = 0) in vec2 texcoord;
layout (location = 0) out vec4 out_fragColor;
in vec4 gl_FragCoord;

void main() {
  float t = texture(sampler2D(atlas_tex,samp), texcoord).x;
  out_fragColor = vec4(string_consts.color.rgb, t);
}
