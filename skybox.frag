#version 450
#pragma shader_stage(fragment)

layout (set = 0, binding = 2) uniform textureCube tex;
layout (set = 0, binding = 3) uniform sampler samp;

in vec4 gl_FragCoord;
layout (location = 0) in vec3 texcoord;
layout (location = 0) out vec4 out_fragColor;

void main() {
  out_fragColor = texture(samplerCube(tex,samp), texcoord);
}
