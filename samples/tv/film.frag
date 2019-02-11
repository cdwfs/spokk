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
  vec4 film_params;  // x: noiseIntensity, y: scanlineIntensity, z: sCount, w: output_grayscale
  vec4 snow_params;  // x: snowAmount, y: snowSize, zw: unused
} tv_consts;
layout (input_attachment_index = 0, set = 0, binding = 6) uniform subpassInput inputColor;

float snow_rand(vec2 co) {
  return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

void main() {
  // Extract relevant uniforms
  float time = scene_consts.res_and_time.w;
  float nIntensity = tv_consts.film_params.x;
  float sIntensity = tv_consts.film_params.y;
  float sCount = tv_consts.film_params.z;
  bool grayscale = (tv_consts.film_params.w != 0.0);
  float snowAmount = tv_consts.snow_params.x;
  float snowSize = tv_consts.snow_params.y;

  // sample the source
  vec4 cTextureScreen = subpassLoad(inputColor);
  
  // make some noise
  float x = texcoord.x * texcoord.y * time *  1000.0;
  x = mod( x, 13.0 ) * mod( x, 123.0 );
  float dx = mod( x, 0.01 );

  // add noise
  vec3 cResult = cTextureScreen.rgb + cTextureScreen.rgb * clamp( 0.1 + dx * 100.0, 0.0, 1.0 );

  // get us a sine and cosine
  vec2 sc = vec2( sin( texcoord.y * sCount ), cos( texcoord.y * sCount ) );

  // add scanlines
  cResult += cTextureScreen.rgb * vec3( sc.x, sc.y, sc.x ) * sIntensity;

  // interpolate between source and result by intensity
  cResult = cTextureScreen.rgb + clamp( nIntensity, 0.0,1.0 ) * ( cResult - cTextureScreen.rgb );

  // Add static snow
  float xs = floor(gl_FragCoord.x / snowSize);
  float ys = floor(gl_FragCoord.y / snowSize);
  vec4 snow = vec4(snow_rand(vec2(xs * time,ys * time)) * snowAmount);

  // convert to grayscale if desired
  if( grayscale ) {
    cResult = vec3( cResult.r * 0.3 + cResult.g * 0.59 + cResult.b * 0.11 );
  }

  out_fragColor =  vec4( cResult, cTextureScreen.a ) + snow;
}
