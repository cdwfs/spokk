#version 450
#pragma shader_stage(fragment)
layout (location = 0) in vec3 fromEye;
layout (location = 0) out vec4 out_fragColor;
in vec4 gl_FragCoord;

void main() {
  out_fragColor = vec4(1,1,1,1);
}
