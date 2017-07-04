#version 450
#pragma shader_stage(fragment)

layout (set = 0, binding = 4) uniform textureCube skybox_tex;
layout (set = 0, binding = 5) uniform sampler skybox_samp;

in vec4 gl_FragCoord;
layout (location = 0) in vec3 texcoord;
layout (location = 0) out vec4 out_fragColor;

void main() {
  out_fragColor = texture(samplerCube(skybox_tex, skybox_samp), texcoord);
}
