/**
 * @author alteredq / http://alteredqualia.com/
 *
 * Film grain & scanlines shader
 *
 * - ported from HLSL to WebGL / GLSL
 * http://www.truevision3d.com/forums/showcase/staticnoise_colorblackwhite_scanline_shaders-t18698.0.html
 *
 * Screen Space Static Postprocessor
 *
 * Produces an analogue noise overlay similar to a film grain / TV static
 *
 * Original implementation and noise algorithm
 * Pat 'Hawthorne' Shearon
 *
 * Optimized scanlines + noise version with intensity scaling
 * Georg 'Leviathan' Steinrohder
 *
 * This version is provided under a Creative Commons Attribution 3.0 License
 * http://creativecommons.org/licenses/by/3.0/
 *
 * ------------------------------------------------------------------------------
 *
 * @author Felix Turner / www.airtight.cc / @felixturner
 *
 * Static effect. Additively blended digital noise.
 *
 * amount - amount of noise to add (0 - 1)
 * size - size of noise grains (pixels)
 *
 * ------------------------------------------------------------------------------
 *
 * @author felixturner / http://airtight.cc/
 *
 * RGB Shift Shader
 * Shifts red and blue channels from center in opposite directions
 * Ported from http://kriss.cx/tom/2009/05/rgb-shift/
 * by Tom Butterworth / http://kriss.cx/tom/
 *
 * amount: shift distance (1 is width of input)
 * angle: shift angle in radians
 *
 * The MIT License
 * 
 * Copyright (c) 2014 Felix Turner
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * 
 */

#version 450
#pragma shader_stage(fragment)
layout (location = 0) in vec2 texcoord;
in vec4 gl_FragCoord;
layout (location = 0) out vec4 out_fragColor;

layout (set = 0, binding = 0) uniform SceneUniforms {
  vec4 res_and_time;  // xy: viewport resolution in pixels, z: unused, w: elapsed seconds
  vec4 eye_pos_ws;    // xyz: world-space eye position
  vec4 eye_dir_wsn;   // xyz: world-space eye direction (normalized)
  // truncated; this should really be in a header file
} scene_consts;
layout (set = 0, binding = 5) uniform TvUniforms {
  vec4 film_params;  // x: noiseIntensity, y: scanlineIntensity, z: scanlineCount, w: output_grayscale
  vec4 snow_params;  // x: snowAmount, y: snowSize, zw: unused
  vec4 rgb_shift_params; // x: rgbShiftAmount, y: rgbShiftAngle, zw: unused
  vec4 distort_params;  // x: distortionCoarse, y: distortionFine, z: distortionSpeed, w: rollSpeed
} tv_consts;
layout(set = 0, binding = 6) uniform texture2D fbColor;
layout(set = 0, binding = 7) uniform texture2D fbDepth;
layout(set = 0, binding = 8) uniform sampler fbSamp;

float snow_rand(vec2 co) {
  return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

// Start Ashima 2D Simplex Noise
vec3 mod289(vec3 x) {
  return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec2 mod289(vec2 x) {
  return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec3 permute(vec3 x) {
  return mod289(((x*34.0)+1.0)*x);
}

float snoise(vec2 v) {
  const vec4 C = vec4(0.211324865405187,  // (3.0-sqrt(3.0))/6.0",
                      0.366025403784439,  // 0.5*(sqrt(3.0)-1.0)",
                     -0.577350269189626,  // -1.0 + 2.0 * C.x",
                      0.024390243902439); // 1.0 / 41.0",
  vec2 i  = floor(v + dot(v, C.yy) );
  vec2 x0 = v -   i + dot(i, C.xx);

  vec2 i1;
  i1 = (x0.x > x0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
  vec4 x12 = x0.xyxy + C.xxzz;
 x12.xy -= i1;

  i = mod289(i); // Avoid truncation effects in permutation
  vec3 p = permute( permute( i.y + vec3(0.0, i1.y, 1.0 ))
    + i.x + vec3(0.0, i1.x, 1.0 ));

  vec3 m = max(0.5 - vec3(dot(x0,x0), dot(x12.xy,x12.xy), dot(x12.zw,x12.zw)), 0.0);
  m = m*m ;
  m = m*m ;

  vec3 x = 2.0 * fract(p * C.www) - 1.0;
  vec3 h = abs(x) - 0.5;
  vec3 ox = floor(x + 0.5);
  vec3 a0 = x - ox;

  m *= 1.79284291400159 - 0.85373472095314 * ( a0*a0 + h*h );

  vec3 g;
  g.x  = a0.x  * x0.x  + h.x  * x0.y;
  g.yz = a0.yz * x12.xz + h.yz * x12.yw;
  return 130.0 * dot(m, g);
}

// End Ashima 2D Simplex Noise

void main() {
  // Extract relevant uniforms
  float time = scene_consts.res_and_time.w;
  float nIntensity = tv_consts.film_params.x;
  float sIntensity = tv_consts.film_params.y;
  float sCount = tv_consts.film_params.z;
  bool grayscale = (tv_consts.film_params.w != 0.0);
  float snowAmount = tv_consts.snow_params.x;
  float snowSize = tv_consts.snow_params.y;
  float rgbShiftAmount = tv_consts.rgb_shift_params.x;
  float rgbShiftAngle = tv_consts.rgb_shift_params.y;
  float distortionCoarse = tv_consts.distort_params.x;
  float distortionFine = tv_consts.distort_params.y;
  float distortionSpeed = tv_consts.distort_params.z;
  float rollSpeed = tv_consts.distort_params.w;

  // Apply distortion/roll to the input UV
  vec2 p = texcoord;
  float ty = time * distortionSpeed;
  float yt = p.y - ty;
  //smooth distortion
  float distortUvOffset = snoise(vec2(yt*3.0,0.0))*0.2;
  // boost distortion
  distortUvOffset = distortUvOffset * distortionCoarse * distortUvOffset * distortionCoarse * distortUvOffset;
  //add fine grain distortion
  distortUvOffset += snoise(vec2(yt*50.0, 0.0)) * distortionFine * 0.001;
  //combine distortion on X with roll on Y
  vec2 uvBase = vec2(fract(p.x + distortUvOffset), fract(p.y - time * rollSpeed));

  // sample the source, using RGB shift to offset the R and B channels
  // Disable this effect if grayscale conversion is active.
  vec4 cInput;
  if (grayscale) {
    cInput = texture(sampler2D(fbColor, fbSamp), uvBase);
  } else {
    vec2 shiftUvOffset = rgbShiftAmount * vec2(cos(rgbShiftAngle), sin(rgbShiftAngle));
    vec4 cInputR = texture(sampler2D(fbColor, fbSamp), uvBase + shiftUvOffset);
    vec4 cInputGA = texture(sampler2D(fbColor, fbSamp), uvBase);
    vec4 cInputB = texture(sampler2D(fbColor, fbSamp), uvBase - shiftUvOffset);
    cInput = vec4(cInputR.r, cInputGA.g, cInputB.b, cInputGA.a);
  }
  // make some noise
  float x = uvBase.x * uvBase.y * time *  1000.0;
  x = mod( x, 13.0 ) * mod( x, 123.0 );
  float dx = mod( x, 0.01 );

  // add noise
  vec3 cResult = cInput.rgb + cInput.rgb * clamp( 0.1 + dx * 100.0, 0.0, 1.0 );

  // get us a sine and cosine
  vec2 sc = vec2( sin( uvBase.y * sCount ), cos( uvBase.y * sCount ) );

  // add scanlines
  cResult += cInput.rgb * vec3( sc.x, sc.y, sc.x ) * sIntensity;

  // interpolate between source and result by intensity
  cResult = cInput.rgb + clamp( nIntensity, 0.0,1.0 ) * ( cResult - cInput.rgb );

  // Add static snow
  float xs = floor(gl_FragCoord.x / snowSize);
  float ys = floor(gl_FragCoord.y / snowSize);
  vec4 snow = vec4(snow_rand(vec2(xs * time,ys * time)) * snowAmount);

  // convert to grayscale if desired
  if( grayscale ) {
    cResult = vec3( cResult.r * 0.3 + cResult.g * 0.59 + cResult.b * 0.11 );
  }

  out_fragColor =  vec4( cResult, cInput.a ) + snow;
}
