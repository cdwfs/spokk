#version 450
#pragma shader_stage(vertex)

#include <spokk_shader_interface.h>

layout (location = SPOKK_VERTEX_ATTRIBUTE_LOCATION_POSITION) in vec3 pos;

layout (set = 0, binding = 1) uniform MeshUniforms {
  mat4 o2w;
  vec4 tint_color;
} mesh_consts;


void main() {
  vec4 posw = mesh_consts.o2w * vec4(pos,1);

  gl_Position = camera.view_proj * posw;
}
