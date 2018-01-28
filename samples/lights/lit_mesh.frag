#version 450
#pragma shader_stage(fragment)
layout (location = 0) in vec3 pos_ws;
layout (location = 1) in vec3 norm_ws;
layout (location = 2) in vec2 uv;
layout (location = 0) out vec4 out_fragColor;
in vec4 gl_FragCoord;

layout (set = 0, binding = 0) uniform SceneUniforms {
  vec4 time_and_res;  // x: elapsed seconds, yz: viewport resolution in pixels
  vec4 eye_pos_ws;    // xyz: world-space eye position
  vec4 eye_dir_wsn;   // xyz: world-space eye direction (normalized)
  // truncated; this should really be in a header file
} scene_consts;
layout (set = 0, binding = 2) uniform LightUniforms {
  vec4 hemi_down_color;  // xyz: RGB color, w: intensity
  vec4 hemi_up_color;  // xyz: RGB color

  vec4 dir_color;  // xyz: RGB color, w: intensity
  vec4 dir_to_light_wsn; // xyz: world-space normalized vector towards light

  vec4 point_pos_ws_inverse_range; // xyz: world-space light pos, w: inverse range of light
  vec4 point_color;  // xyz: RGB color, w: intensity

  vec4 spot_pos_ws_inverse_range;  // xyz: world-space light pos, w: inverse range of light
  vec4 spot_color;  // xyz: RGB color, w: intensity
  vec4 spot_neg_dir_wsn;  // xyz: world-space normalized light direction (negated)
  vec4 spot_falloff_angles;  // x: 1/(cos(inner)-cos(outer)), y: cos(outer)
} light_consts;

layout (set = 0, binding = 3) uniform MaterialUniforms {
  vec4 albedo;  // xyz: albedo RGB
  vec4 emissive_color;  // xyz: emissive color, w: intensity
  vec4 spec_color;  // xyz: specular RGB, w: intensity
  vec4 spec_exp;  // x: specular exponent
  vec4 ggx_params0;  // x: metallic, y: subsurface, z: specular, w: roughness
  vec4 ggx_params1;  // x: specularTint, y: anisotropic, z: sheen, w: sheenTint
  vec4 ggx_params2;  // x: clearcoat, y: clearcoatGloss {0.0, 1.0, __, __}
} mat_consts;


#include <common/cookbook.glsl>

const float PI = 3.14159265358979323846;

float sqr(float x) { return x*x; }

float SchlickFresnel(float u)
{
  float m = clamp(1-u, 0, 1);
  float m2 = m*m;
  return m2*m2*m; // pow(m,5)
}
float GTR1(float NdotH, float a)
{
  if (a >= 1) return 1/PI;
  float a2 = a*a;
  float t = 1 + (a2-1)*NdotH*NdotH;
  return (a2-1) / (PI*log(a2)*t);
}
float GTR2(float NdotH, float a)
{
  float a2 = a*a;
  float t = 1 + (a2-1)*NdotH*NdotH;
  return a2 / (PI * t*t);
}
float GTR2_aniso(float NdotH, float HdotX, float HdotY, float ax, float ay)
{
  return 1 / (PI * ax*ay * sqr( sqr(HdotX/ax) + sqr(HdotY/ay) + NdotH*NdotH ));
}
float smithG_GGX(float NdotV, float alphaG)
{
  float a = alphaG*alphaG;
  float b = NdotV*NdotV;
  return 1 / (NdotV + sqrt(a + b - a*b));
}
float smithG_GGX_aniso(float NdotV, float VdotX, float VdotY, float ax, float ay)
{
  return 1 / (NdotV + sqrt( sqr(VdotX*ax) + sqr(VdotY*ay) + sqr(NdotV) ));
}
vec3 mon2lin(vec3 x)
{
  return vec3(pow(x[0], 2.2), pow(x[1], 2.2), pow(x[2], 2.2));
}
vec3 BRDF( vec3 L, vec3 V, vec3 N, vec3 X, vec3 Y )
{
  const vec3 baseColor = mat_consts.albedo.xyz;
  const float metallic = mat_consts.ggx_params0.x;
  const float subsurface = mat_consts.ggx_params0.y;
  const float specular = mat_consts.ggx_params0.z;
  const float roughness = mat_consts.ggx_params0.w;
  const float specularTint = mat_consts.ggx_params1.x;
  const float anisotropic = mat_consts.ggx_params1.y;
  const float sheen = mat_consts.ggx_params1.z;
  const float sheenTint = mat_consts.ggx_params1.w;
  const float clearcoat = mat_consts.ggx_params2.x;
  const float clearcoatGloss = mat_consts.ggx_params2.y;

  float NdotL = dot(N,L);
  float NdotV = dot(N,V);
  if (NdotL < 0 || NdotV < 0) return vec3(0);

  vec3 H = normalize(L+V);
  float NdotH = dot(N,H);
  float LdotH = dot(L,H);

  vec3 Cdlin = mon2lin(baseColor);
  float Cdlum = .3*Cdlin[0] + .6*Cdlin[1]  + .1*Cdlin[2]; // luminance approx.

  vec3 Ctint = Cdlum > 0 ? Cdlin/Cdlum : vec3(1); // normalize lum. to isolate hue+sat
  vec3 Cspec0 = mix(specular*.08*mix(vec3(1), Ctint, specularTint), Cdlin, metallic);
  vec3 Csheen = mix(vec3(1), Ctint, sheenTint);

  // Diffuse fresnel - go from 1 at normal incidence to .5 at grazing
  // and mix in diffuse retro-reflection based on roughness
  float FL = SchlickFresnel(NdotL), FV = SchlickFresnel(NdotV);
  float Fd90 = 0.5 + 2 * LdotH*LdotH * roughness;
  float Fd = mix(1.0, Fd90, FL) * mix(1.0, Fd90, FV);

  // Based on Hanrahan-Krueger brdf approximation of isotropic bssrdf
  // 1.25 scale is used to (roughly) preserve albedo
  // Fss90 used to "flatten" retroreflection based on roughness
  float Fss90 = LdotH*LdotH*roughness;
  float Fss = mix(1.0, Fss90, FL) * mix(1.0, Fss90, FV);
  float ss = 1.25 * (Fss * (1 / (NdotL + NdotV) - .5) + .5);

  // specular
  float aspect = sqrt(1-anisotropic*.9);
  float ax = max(.001, sqr(roughness)/aspect);
  float ay = max(.001, sqr(roughness)*aspect);
  float Ds = GTR2_aniso(NdotH, dot(H, X), dot(H, Y), ax, ay);
  float FH = SchlickFresnel(LdotH);
  vec3 Fs = mix(Cspec0, vec3(1), FH);
  float Gs;
  Gs  = smithG_GGX_aniso(NdotL, dot(L, X), dot(L, Y), ax, ay);
  Gs *= smithG_GGX_aniso(NdotV, dot(V, X), dot(V, Y), ax, ay);

  // sheen
  vec3 Fsheen = FH * sheen * Csheen;

  // clearcoat (ior = 1.5 -> F0 = 0.04)
  float Dr = GTR1(NdotH, mix(.1,.001,clearcoatGloss));
  float Fr = mix(.04, 1.0, FH);
  float Gr = smithG_GGX(NdotL, .25) * smithG_GGX(NdotV, .25);

  return ((1/PI) * mix(Fd, ss, subsurface)*Cdlin + Fsheen)
    * (1-metallic)
    + Gs*Fs*Ds + .25*clearcoat*Gr*Fr*Dr;
}

mat3 cotangent_frame( vec3 N, vec3 p, vec2 uv )
{
  // get edge vectors of the pixel triangle
  vec3 dp1 = dFdx( p );
  vec3 dp2 = dFdy( p );
  vec2 duv1 = dFdx( uv );
  vec2 duv2 = dFdy( uv );

  // solve the linear system
  vec3 dp2perp = cross( dp2, N );
  vec3 dp1perp = cross( N, dp1 );
  vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
  vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;

  // construct a scale-invariant frame 
  float invmax = inversesqrt( max( dot(T,T), dot(B,B) ) );
  return mat3( T * invmax, B * invmax, N );
}

// Compute tangent frame from normal + "up" vector
void computeTangentVectors( vec3 inVec, out vec3 uVec, out vec3 vVec )
{
  uVec = abs(inVec.x) < 0.999 ? vec3(1,0,0) : vec3(0,1,0);
  uVec = normalize(cross(inVec, uVec));
  vVec = normalize(cross(inVec, uVec));
}

void main() {
  Material mat;
  mat.albedo_color = mat_consts.albedo.xyz;
  mat.normal_wsn = normalize(norm_ws);
  mat.spec_color = mat_consts.spec_color.xyz;
  mat.spec_intensity = mat_consts.spec_color.w;
  mat.spec_exp = mat_consts.spec_exp.x;
#if 0
  HemiLight hemi_light;
  hemi_light.down_color = light_consts.hemi_down_color.xyz;
  hemi_light.up_color = light_consts.hemi_up_color.xyz;
  vec3 hemi_color = light_consts.hemi_down_color.w * ApplyHemiLight(mat, hemi_light);

  DirLight dir_light;
  dir_light.to_light_wsn = light_consts.dir_to_light_wsn.xyz;
  dir_light.color = light_consts.dir_color.xyz;
  vec3 dir_color = light_consts.dir_color.w * ApplyDirLight(pos_ws, scene_consts.eye_pos_ws.xyz, mat, dir_light);

  PointLight point_light;
  point_light.pos_ws = light_consts.point_pos_ws_inverse_range.xyz;
  point_light.inverse_range = light_consts.point_pos_ws_inverse_range.w;
  point_light.color = light_consts.point_color.xyz;
  vec3 point_color = light_consts.point_color.w * ApplyPointLight(pos_ws, scene_consts.eye_pos_ws.xyz, mat, point_light);

  SpotLight spot_light;
  spot_light.pos_ws = light_consts.spot_pos_ws_inverse_range.xyz;
  spot_light.inverse_range = light_consts.spot_pos_ws_inverse_range.w;
  spot_light.color = light_consts.spot_color.xyz;
  spot_light.neg_light_dir_wsn = light_consts.spot_neg_dir_wsn.xyz;
  spot_light.inv_inner_outer = light_consts.spot_falloff_angles.x;
  spot_light.cosine_outer = light_consts.spot_falloff_angles.y;
  vec3 spot_color = light_consts.spot_color.w * ApplySpotLight(pos_ws, scene_consts.eye_pos_ws.xyz, mat, spot_light);

  vec3 emissive_color = mat_consts.emissive_color.xyz * mat_consts.emissive_color.w;

  out_fragColor.xyz = hemi_color + dir_color + point_color + spot_color + emissive_color;
  out_fragColor.w = 1;
#else
  //mat3 TBN = cotangent_frame(norm_ws, pos_ws, uv);
  //vec3 N = normalize(norm_ws);
  //vec3 T = normalize(tan_ws);
  //vec3 B = normalize(bitan_ws);
  vec3 N = normalize(norm_ws);
  vec3 T, B;
  computeTangentVectors(N, T, B);
  vec3 L = normalize(light_consts.point_pos_ws_inverse_range.xyz - pos_ws);//light_consts.dir_to_light_wsn.xyz;
  vec3 V = normalize(scene_consts.eye_pos_ws.xyz - pos_ws);
  vec3 c = max(BRDF(L, V, N, T, B), vec3(0));
  c *= dot(L, N);
  out_fragColor = vec4(c, 1.0);
  //out_fragColor = vec4(0.5*B+vec3(0.5), 1.0);
  //out_fragColor = vec4(uv, 0, 1);
#endif
}
