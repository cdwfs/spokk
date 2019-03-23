#version 450
#pragma shader_stage(vertex)
void main(void) {
  // Select your winding order:
  vec2 uv     = vec2((gl_VertexIndex<<1) & 2, gl_VertexIndex & 2); // CW -- [-1,1], [3,1], -1,-3]
  //vec2 uv     = vec2(gl_VertexIndex & 2, (gl_VertexIndex<<1) & 2); // CCW -- [-1,1], [-1,-3], [3,1]
  gl_Position = vec4(uv * vec2(2,-2) + vec2(-1,1), 0, 1);
}
