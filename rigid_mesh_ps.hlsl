#pragma shader_stage(fragment)

Texture2D tex : register(t2);
SamplerState samp : register(s3);


struct VS_OUTPUT {
  float4 pos : SV_POSITION;
  float2 texcoord : TEXCOORD0;
  float3 norm : TEXCOORD1;
  float3 fromEye : TEXCOORD2;
};

float4 main(VS_OUTPUT input) : COLOR {
  float4 head_light = dot(normalize(-input.fromEye), normalize(input.norm)) * float4(1,1,1,1);
  return head_light * tex.Sample(samp, input.texcoord);
}
