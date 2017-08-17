struct VSInput {
  float4 Position : POSITION;
  float2 TexCoord : TEXCOORD0;
  float3 Normal   : NORMAL;
};

struct PSInput {
  float4 Position      : SV_POSITION;
  float3 Normal        : NORMAL;
  float2 TexCoord      : TEXCOORD0;
  float3 ViewDirection : TEXCOORD1;
};

cbuffer uBlock0T : register(b0)
{
  float4x4 MatrixModel;
  float4x4 MatrixView;
  float4x4 MatrixProj;
  float3x3 MatrixNormal;
};

Texture2D<float4> Tex  : register(t1);
sampler           Samp : register(s2);

PSInput VSMain(VSInput input)
{
  PSInput ret;

  float4 modelPosition = mul(MatrixModel, input.Position);
  float4 viewPosition  = mul(MatrixView, modelPosition);
  ret.Position         = mul(MatrixProj, viewPosition);
  ret.Normal           = normalize(mul(MatrixNormal, input.Normal));
  ret.TexCoord         = input.TexCoord;
  ret.ViewDirection    = normalize(-viewPosition.xyz / viewPosition.w);

  return ret;
}

// N - normal
// L - light direction
float lambert(float3 N, float3 L)
{
  float result = max(0.0, dot(N, L));
  return result;
}

float4 PSMain(PSInput input) : SV_Target0
{
  float A = 0.2;
  float D = lambert(input.Normal, input.ViewDirection);
  float4 Ct = Tex.Sample(Samp, input.TexCoord);

  // Color output
  float3 Co = Ct.rgb * (A + D);

  return float4(Co.rgb, 1);
}
