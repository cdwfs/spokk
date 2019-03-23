#pragma shader_stage(vertex)

cbuffer SceneUniforms : register(b0) {
  float4 res_and_time;
  float4 eye;
  float4x4 viewproj;
};
cbuffer ObjectToWorld : register(b1) {
  float4 matrix_columns[4*1024];
};

struct VS_INPUT {
  float3 pos : POSITION;
  float3 normal : NORMAL;
  float2 attr : TEXCOORD0;
  uint instance : SV_InstanceID;
};
struct VS_OUTPUT {
  float4 pos : SV_POSITION;
  float2 texcoord : TEXCOORD0;
  float3 norm : TEXCOORD1;
  float3 fromEye : TEXCOORD2;
};

VS_OUTPUT main(VS_INPUT input) {
  VS_OUTPUT output;

  output.texcoord = input.attr;
  float4x4 o2w = float4x4(
    matrix_columns[4*input.instance+0],
    matrix_columns[4*input.instance+1],
    matrix_columns[4*input.instance+2],
    matrix_columns[4*input.instance+3]);

  float3x3 n2w = float3x3(o2w);
  output.norm = mul(n2w, input.normal);

  float4 posw = o2w * float4(input.pos, 1);
  output.fromEye = posw.xyz - eye.xyz;
  output.pos = mul(viewproj, posw);

  return output;
}
