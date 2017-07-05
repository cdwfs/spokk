// How to draw:
// topology: triangle list
// draw 36 indices (no index/vertex buffer necessary)
// disable depth write
// enable depth test 
// scale cube vertices by zfar/sqrt(3) or roughly 0.575*zfar

#pragma shader_stage(vertex)

const uint indices[6*6] = {
  0,1,2, 3,2,1, // -Z
  4,0,6, 2,6,0, // -X
  1,5,3, 7,3,5, // +X
  5,4,7, 6,7,4, // +Z
  4,5,0, 1,0,5, // -Y
  2,3,6, 7,6,3, // +Y
};

cbuffer SceneUniforms : register(b0) {
  float4 time_and_res;
  float4 eye;
  float4x4 viewproj;
  float4x4 view;
  float4x4 proj;
  float4 zrange; // .x=near, .y=far, .zw=unused
} scene_consts;

struct VS_OUTPUT {
  float4 pos : SV_POSITION;
  float3 uv : TEXCOORD0;
};

VS_OUTPUT main(uint vertex_id : SV_VertexID) {
  VS_OUTPUT output;

  uint index = indices[vertex_id];
  output.uv = float3(
    (index & 1) ? 1.0 : -1.0,
    (index & 2) ? 1.0 : -1.0,
    (index & 4) ? 1.0 : -1.0);

  float3x3 view_rot = float3x3(scene_consts.view);
  const float inv_sqrt3 = 0.575;  // slightly low to ensure corners are always closer than zfar
  output.pos = scene_consts.proj * float4(view_rot * (0.575 * scene_consts.zrange.y * output.uv), 1.0);

  return output;
}
