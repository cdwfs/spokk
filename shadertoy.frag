#version 450
#pragma shader_stage(fragment)
layout (location = 0) out vec4 out_fragColor;
in vec4 gl_FragCoord;

// input channel.  // TODO: cubemaps?
layout (set = 0, binding = 0) uniform sampler2D iChannel0;
layout (set = 0, binding = 1) uniform sampler2D iChannel1;
layout (set = 0, binding = 2) uniform sampler2D iChannel2;
layout (set = 0, binding = 3) uniform sampler2D iChannel3;
layout (set = 0, binding = 4) uniform ShaderToyUniforms {
  vec3      iResolution;           // viewport resolution (in pixels)
  float     iGlobalTime;           // shader playback time (in seconds)
  float     iTimeDelta;            // render time (in seconds)
  int       iFrame;                // shader playback frame
  float     iChannelTime[4];       // channel playback time (in seconds)
  vec3      iChannelResolution[4]; // channel resolution (in pixels)
  vec4      iMouse;                // mouse pixel coords. xy: current (if MLB down), zw: click
  vec4      iDate;                 // (year, month, day, time in seconds)
  float     iSampleRate;           // sound sample rate (i.e., 44100
};

void mainImage(out vec4 fragColor, in vec2 fragCoord);
void main() {
	mainImage(out_fragColor, gl_FragCoord.xy);
}

////////////////////////////////////////////////////////////////////////////////////////////////

vec3 translate(in vec3 pos, in vec3 offset)
{
	return pos - offset;
}

vec3 rotateX(in vec3 pos, in float rad)
{
	float c = cos(-rad), s = sin(-rad);
	mat3 m = mat3(1,0,0,	0,c,s,	0,-s,c);
	return m*pos;
}
vec3 rotateY(in vec3 pos, in float rad)
{
	float c = cos(-rad), s = sin(-rad);
	mat3 m = mat3(c,0,-s,	0,1,0,	s,0,c);
	return m*pos;
}
vec3 rotateZ(in vec3 pos, in float rad)
{
	float c = cos(-rad), s = sin(-rad);
	mat3 m = mat3(c,s,0,	-s,c,0,		0,0,1);
	return m*pos;
}

vec3 scale(in vec3 pos, in vec3 scale)
{
	return vec3(pos.x/scale.x, pos.y/scale.y, pos.z/scale.z);
}

float sphere(in vec3 pos, in float radius)
{
	return length(pos) - radius;
}

float box(in vec3 pos, in vec3 halfExtents)
{
	vec3 a = abs(pos) - halfExtents;
	return max(a.x, max(a.y, a.z));
}

float sdMetaBalls( vec3 pos ) // adapted from https://www.shadertoy.com/view/ld2GRz
{
	int numIntersections = 0;
	float p = 0.0;
	float dmin = 1e20;
	int numballs = 4;
	float radius = 2.0;
		
	float lipschitz = 1.0; // track Lipschitz constant. Lower = higher detail?
	
	for( int i=0; i<numballs; i++ )
	{
		vec3 center = 1.0*vec3(sin(10*i+iGlobalTime), cos(20*i+iGlobalTime), sin(30*i+iGlobalTime));
        // bounding sphere for each ball
        float db = length( center - pos );
        if( db < radius )
    	{
            // evaluate metaball
    		float x = db/radius;
    		p += 1.0 - x*x*x*(x*(x*6.0-15.0)+10.0);
	    	numIntersections += 1;
    		lipschitz = max( lipschitz, 0.5333*radius );
	    }
	    else // bouncing sphere distance
	    {
    		dmin = min( dmin, db - radius );
    	}
	}

    float d = dmin + 0.01;
	
	if( numIntersections > 0 )
	{
		float threshold = 0.2;
		d = lipschitz*(threshold-p);
	}
	
	return d;
}


float map(in vec3 pos)
{
	float d = 1000000.0;

    // rotating box
	d = min( d, box( rotateY(scale(pos,vec3(3,3,3)), iGlobalTime), vec3(.1,1,1)) );

    // multi-balls
    for(int i=0; i<4; ++i)
    {
        vec3 p = translate(pos, 1.0*vec3(sin(10*i+iGlobalTime), cos(20*i+iGlobalTime), sin(30*i+iGlobalTime)));
	    //d = min( d, sphere(p, 2) );
    }

    // metaballs
    d = min( d, sdMetaBalls(pos) );

    // instanced balls
	//d = min( d, sphere(translate(vec3(mod(pos.x,1)-0.5, mod(pos.y,1)-0.5, mod(pos.z,1)-0.5), vec3(0,0,0)), 0.1) );

	return d;
}

float raymarch( in vec3 ro, in vec3 rd )
{
	float maxd = 30.0; // maximum distance to march before we give up
	float precis = 0.001; // how close to a surface do we consider a hit?
    float t = 0.1; // distance along ray
    float h = 1.0; // distance to surface at time t
    for(int i=0; i<160; i++)
    {
        if (h < precis || t > maxd)
            break;
	    h = map(ro +rd*t);
        t += h;
    }

    if(t > maxd)
		t = -1.0; // no hit
    return t;
}

vec3 calcNormal(in vec3 pos)
{
	// Compute the gradient of the distance field at the specified point.
	// On the surface, that's a good approximation of the normal vector
    vec3 eps = vec3(0.02,0.0,0.0);
	return normalize(vec3(
           map(pos+eps.xyy) - map(pos-eps.xyy),
           map(pos+eps.yxy) - map(pos-eps.yxy),
           map(pos+eps.yyx) - map(pos-eps.yyx) ));
}

// Fake a cubemap where all six faces are the same. Mostly useful for noise textures.
vec4 texcube( sampler2D sam, in vec3 p, in vec3 n )
{
	vec4 x = texture( sam, p.yz ).xxxx;
	vec4 y = texture( sam, p.zx ).xxxx;
	vec4 z = texture( sam, p.xy ).xxxx;
	return x*abs(n.x) + y*abs(n.y) + z*abs(n.z);
}

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    //vec3 rayOrigin = vec3(sin(iGlobalTime), 3*cos(0.4*iGlobalTime), 10); // ray origin
	vec3 rayOrigin = vec3(0,0,10);

	// camera tx
    float roll = 0;
	vec3 cameraForward = vec3(0, 0, -1); // normalize(ta-rayOrigin);
	vec3 cameraUp = vec3(sin(roll), cos(roll),0.0);
	vec3 cameraRight = normalize(cross(cameraForward,cameraUp));
	cameraUp = normalize(cross(cameraRight,cameraForward));

	// p is (0,0) at the center of the screen,
	// and ranges [-aspectRatio..aspectRatio] in x and [-1..1] in y.
	vec2 p = -1.0 + 2.0*(fragCoord.xy / iResolution.xy);
	p.x *= iResolution.x / iResolution.y;
	
	// Cryptic, magical math to adjust p based on distance from the middle of the screen.
	// Perspective correction? Lens simulation? Gonna leave it off for now.
	//float r2 = p.x*p.x*0.32 + p.y*p.y;
    //p *= (7.0-sqrt(37.5-11.5*r2))/(r2+1.0);

	vec3 rayDir = normalize( p.x*cameraRight + p.y*cameraUp + 2.1*cameraForward ); // ray direction

    vec3 pixelColor = vec3(0.1, 0.1 ,0.1); // background
	float t = raymarch(rayOrigin, rayDir);
    if(t > 0)
	{
		vec3 pos = rayOrigin + t*rayDir;
		vec3 nor = calcNormal( pos );
		vec3 bn = texcube(iChannel0, pos, nor).xyz;
        pixelColor = normalize(nor + 0.5*bn);
    }
	fragColor = vec4( pixelColor, 1.0 );
}
