#version 450
#pragma shader_stage(vertex)
layout (location = 0) in vec2 pos_vp;
layout (location = 1) in vec2 attr;
layout (location = 0) out vec2 texcoord;

layout (set = 0, binding = 0) uniform StringUniforms {
  vec4 color;
  vec4 viewport_to_clip;
} string_consts;

void main() {
  texcoord.xy = attr.xy;
  gl_Position = vec4(
    pos_vp.x * string_consts.viewport_to_clip.x + string_consts.viewport_to_clip.z,
    pos_vp.y * string_consts.viewport_to_clip.y + string_consts.viewport_to_clip.w,
    0, 1);
}
