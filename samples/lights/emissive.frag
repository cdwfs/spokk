#version 450
#pragma shader_stage(fragment)
layout (location = 0) out vec4 out_fragColor;
in vec4 gl_FragCoord;

layout (set = 0, binding = 1) uniform MeshUniforms {
  mat4 o2w;
  vec4 tint_color;
} mesh_consts;

void main() {
  out_fragColor = mesh_consts.tint_color;
}
