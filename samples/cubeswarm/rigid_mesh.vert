#version 450
#pragma shader_stage(vertex)

#include <spokk_shader_interface.h>

layout (location = SPOKK_VERTEX_ATTRIBUTE_LOCATION_POSITION) in vec3 pos;
layout (location = SPOKK_VERTEX_ATTRIBUTE_LOCATION_NORMAL) in vec3 normal;
layout (location = SPOKK_VERTEX_ATTRIBUTE_LOCATION_TEXCOORD0) in vec2 attr;
layout (location = 0) out vec2 texcoord;
layout (location = 1) out vec3 norm;
layout (location = 2) out vec3 fromEye;

void main() {
  texcoord = attr;

  mat3 n2w = mat3(transforms.world);
  norm = n2w * normal;
    
  vec4 posw = transforms.world * vec4(pos,1);
  fromEye = posw.xyz - camera.eye_pos_ws.xyz;
  vec4 outpos = camera.view_proj * posw;
  gl_Position = outpos;
}
