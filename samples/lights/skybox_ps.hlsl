#pragma shader_stage(fragment)

TextureCube tex : register(t2);
SamplerState samp : register(s3);


struct VS_OUTPUT {
  float4 pos : SV_POSITION;
  float3 texcoord : TEXCOORD0;
};

float4 main(VS_OUTPUT input) : COLOR {
  return tex.Sample(samp, input.texcoord);
}
