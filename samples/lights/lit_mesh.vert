#version 450
#pragma shader_stage(vertex)

#include <spokk_shader_interface.h>

layout (location = SPOKK_VERTEX_ATTRIBUTE_LOCATION_POSITION) in vec3 pos;
layout (location = SPOKK_VERTEX_ATTRIBUTE_LOCATION_NORMAL) in vec3 normal;
layout (location = 0) out vec3 pos_ws;
layout (location = 1) out vec3 norm_ws;

layout (set = 0, binding = 1) uniform MeshUniforms {
  mat4 o2w;
} mesh_consts;


void main() {
  mat3 n2w = mat3(mesh_consts.o2w);
  vec4 posw = mesh_consts.o2w * vec4(pos,1);

  pos_ws = posw.xyz;
  norm_ws = n2w * normal;
  gl_Position = camera.viewproj * posw;
}
