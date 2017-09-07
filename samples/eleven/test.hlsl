struct VSInput {
  uint index : SV_VertexID;
};

struct PSInput {
  float4 position : SV_POSITION;
  float3 color : COLOR;
};

PSInput VSMain(VSInput input) {
  const float4 positions[3] = {
      float4(-0.5, -0.5, 0.0, 1.0),
      float4(+0.5, -0.5, 0.0, 1.0),
      float4(+0.0, +0.5, 0.0, 1.0),
  };
  const float3 colors[3] = {
      float3(1.0, 0.0, 0.0),
      float3(0.0, 1.0, 0.0),
      float3(0.0, 0.0, 1.0),
  };
  PSInput ret;

  ret.position = positions[input.index];
  ret.color = colors[input.index];
  return ret;
}

float4 PSMain(PSInput input) : SV_Target0 { return float4(input.color, 1); }
