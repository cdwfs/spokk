#version 450
#pragma shader_stage(vertex)
layout (location = 0) in ivec2 poso;
layout (location = 1) in vec2 attr;
layout (location = 0) out vec2 texcoord;
layout (location = 1) out vec3 tint;

layout (set = 0, binding = 0) uniform SceneUniforms {
  vec4 time_and_res;
  vec4 eye;
  mat4 viewproj;
};
layout (set = 0, binding = 2) uniform StringUniforms {
  mat4 o2w;
};

// pos = origin + velocity * (time - spawn_time)

void main() {
  texcoord.xy = attr.xy;
  tint = vec3(1, 0.5, 1);

  vec4 posw = o2w * vec4(float(poso.x), -float(poso.y), 0, 1);
  gl_Position = viewproj * posw;
}
