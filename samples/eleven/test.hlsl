struct VSInput {
  float3 position : POSITION;
  float2 texcoord : TEXCOORD0;
  float3 normal : NORMAL;
};

struct PSInput {
  float4 position : SV_POSITION;
  float3 normal : NORMAL;
  float2 texcoord : TEXCOORD0;
};

PSInput VSMain(VSInput input) {
  PSInput ret;
  ret.position = float4(input.position, 1);
  ret.normal = input.normal;
  ret.texcoord = input.texcoord;
  return ret;
}

float4 PSMain(PSInput input) : SV_Target0 { return float4(input.texcoord, 0, 1); }
