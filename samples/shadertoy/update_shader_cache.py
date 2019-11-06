import hashlib
import json
import os
import os.path
import shutil
import subprocess
import sys
import urllib.request

def fetch_shader_info(shader_id, api_key):
    url = "https://www.shadertoy.com/api/v1/shaders/%s?key=%s" % (shader_id, api_key)
    with urllib.request.urlopen(url) as response:
        response_code = response.getcode()
        if response_code != 200:
            print("ERROR: Unexpected HTTP response: %d" % response_code)
            return {}
        return json.load(response)

def validate_shader_info(shader_info):
    if "Error" in shader_info.keys():
        raise ValueError("Shadertoy says: " + shader_info['Error'])
        
    if "Shader" not in shader_info.keys():
        raise ValueError("Missing 'Shader' key in JSON")
        
    shader_info_version = shader_info['Shader']['ver']
    if shader_info_version != "0.1":
        raise ValueError("Unexpected shader version " + shader_info_version)

def fetch_resource_from_url(url, local_filename):
    with urllib.request.urlopen(url) as response:
        if response.getcode() != 200:
            print("%s: HTTP Error code %d; skipping" % (local_filename, response.getcode()))
            return False
        try:
            input_nbytes = int(response.headers['Content-Length'])
        except:
            input_nbytes = -1
        # Skip updating the local file if it already exists and its hash matches.
        if os.path.exists(local_filename) and os.path.getsize(local_filename) == input_nbytes:
            print("%s exists; skipping" % local_filename)
            return True # TEMP: skip hash check for now, size check is fine
            remote_bytes = bytearray(response.read())
            remote_hash = hashlib.sha256(remote_bytes).hexdigest()
            with open(local_filename, "rb") as local_file:
                local_bytes = bytearray(local_file.read())
            local_hash = hashlib.sha256(local_bytes).hexdigest()
            if remote_hash == local_hash:
                print("%s exists; skipping" % local_filename)
                return True
        with open(local_filename, "wb") as local_file:
            shutil.copyfileobj(response, local_file)
            print("%s: saved" % local_filename)
    return True

def fetch_shader_inputs(shader_info, local_cache_dir):
    local_media_dir = os.path.join(local_cache_dir, "media")
    os.makedirs(local_media_dir, exist_ok=True)
    for renderpass in shader_info['Shader']['renderpass']:
        for input in renderpass['inputs']:
            remote_dirname, remote_filename = os.path.split(input['src'])
            assert remote_dirname == "/media/a", "shader input channel %d has src path outside /media/a ('%s')" % (input['channel'], input['src'])
            if input['ctype'] == "cubemap":
                remote_base, remote_ext = os.path.splitext(remote_filename)
                # TODO(cort): If the sample's flipY is enabled, the +Y and -Y faces are swapped.
                local_srcs = []
                for suffix in ["", "_1", "_2", "_3", "_4", "_5"]:
                    remote_face_filename = remote_base + suffix + remote_ext
                    local_face_filename = os.path.join(local_media_dir, remote_face_filename)
                    url = "https://www.shadertoy.com" + remote_dirname + "/" + remote_face_filename
                    if not fetch_resource_from_url(url, local_face_filename):
                        return False
                    local_srcs.append(os.path.relpath(local_face_filename, start=local_cache_dir))
                input['spokk_local_src'] = local_srcs
            else:
                local_filename = os.path.join(local_media_dir, remote_filename)
                url = "https://www.shadertoy.com" + input['src']
                if not fetch_resource_from_url(url, local_filename):
                    return False
                input['spokk_local_src'] = os.path.relpath(local_filename, start=local_cache_dir)
    return True

shader_header_template = R"""#version 450
#pragma shader_stage(fragment)
layout (location = 0) out vec4 out_fragColor;
in vec4 gl_FragCoord;

// input channel.
layout (set = 0, binding = 0) uniform sampler%s iChannel0;
layout (set = 0, binding = 1) uniform sampler%s iChannel1;
layout (set = 0, binding = 2) uniform sampler%s iChannel2;
layout (set = 0, binding = 3) uniform sampler%s iChannel3;
// input uniforms. NOTE: declaraction order is different from shadertoy due to packing rules
layout (set = 0, binding = 4) uniform ShaderToyUniforms {
  vec3      iResolution;           // viewport resolution (in pixels)
  float     iChannelTime[4];       // channel playback time (in seconds)
  vec3      iChannelResolution[4]; // channel resolution (in pixels)
  vec4      iMouse;                // mouse pixel coords. xy: current (if MLB down), zw: click
  vec4      iDate;                 // (year, month, day, time in seconds)
  float     iTime;                 // shader playback time (in seconds)
  float     iTimeDelta;            // render time (in seconds)
  int       iFrame;                // shader playback frame
  float     iSampleRate;           // sound sample rate (i.e., 44100)
};

void mainImage(out vec4 fragColor, in vec2 fragCoord);
void main() {
  // Need to manually flip the fragcoord to a lower-left origin
  mainImage(out_fragColor, vec2(gl_FragCoord.x, iResolution.y - gl_FragCoord.y));
  out_fragColor.w = 1.0;
}

////////////////////////////////////////////////////////////////////////////////////////////////

"""

def write_shader_source(shader_info, local_cache_dir):
    local_shaders_dir = os.path.join(local_cache_dir, "shaders")
    vulkan_sdk_dir = os.getenv("VULKAN_SDK")
    assert vulkan_sdk_dir, "VULKAN_SDK not set in environment"
    glslc_path = os.path.normpath(os.path.join(vulkan_sdk_dir, "bin/glslc"))
    
    os.makedirs(local_source_dir, exist_ok=True)
    shader_id = shader_info['Shader']['info']['id']
    renderpass_count = len(shader_info['Shader']['renderpass'])
    for i in range(renderpass_count):
        renderpass = shader_info['Shader']['renderpass'][i]
        # determine sampler types based on input types, and generate the final shader header and source code.
        sampler_types = [""] * 4
        for input in renderpass['inputs']:
            if input['ctype'] == 'texture':
                sampler_types[input['channel']] = "2D"
            elif input['ctype'] == 'music':
                sampler_types[input['channel']] = "2D" # still 2D, I guess?
            elif input['ctype'] == 'cubemap':
                sampler_types[input['channel']] = "Cube"
        shader_header = shader_header_template % (sampler_types[0], sampler_types[1], sampler_types[2], sampler_types[3])
        code = shader_header + renderpass['code']
        # Write shader source code to file
        source_filename = os.path.join(local_source_dir, "%s_%d.frag" % (shader_id, i))        
        renderpass['spokk_local_code'] = os.path.relpath(source_filename, start=local_cache_dir)
        # Skip writing source / compiling if file exists and hashes match
        skip_write_source = False
        if os.path.exists(source_filename):
            new_hash = hashlib.sha256(code.encode()).hexdigest()
            with open(source_filename, "r") as local_file:
                local_bytes = local_file.read().encode()
            local_hash = hashlib.sha256(local_bytes).hexdigest()
            if new_hash == local_hash:
                print("%s exists; skipping" % source_filename)
                skip_write_source = True
        if not skip_write_source:
            with open(source_filename, "w") as source_file:
                source_file.write(code)
            print("Saved %s" % source_filename)
        # Compile shader
        spv_filename = source_filename + ".spv"
        skip_compile = False
        if os.path.exists(spv_filename):
            source_mtime = os.path.getmtime(source_filename)
            spv_mtime = os.path.getmtime(spv_filename)
            if source_mtime < spv_mtime:
                print("%s exists and is newer; skipping compilation" % spv_filename)
                skip_compile = True # SPV is newer than source
        if not skip_compile:
            glslc_args = [glslc_path, "-fshader-stage=frag", "-O", "--target-env=vulkan1.1", "-o", spv_filename, source_filename]
            completed = subprocess.run(glslc_args)
            try:
                completed.check_returncode()
            except subprocess.CalledProcessError as e:
                print(e) # TODO(cort): better error message here
                return False
            renderpass['spokk_local_spv'] = os.path.relpath(spv_filename, start=local_cache_dir)
            print("Compiled %s -> %s" % (source_filename, spv_filename))
    return True

def write_shader_info(shader_info, local_cache_dir):
    os.makedirs(local_cache_dir, exist_ok=True)
    shader_id = shader_info['Shader']['info']['id']
    local_filename = os.path.join(local_cache_dir, shader_id + ".json")
    new_json = json.dumps(shader_info, indent=2)
    # Skip writing source / compiling if file exists and hashes match
    if os.path.exists(local_filename):
        new_hash = hashlib.sha256(new_json.encode()).hexdigest()
        with open(local_filename, "r") as local_file:
            local_bytes = local_file.read().encode()
        local_hash = hashlib.sha256(local_bytes).hexdigest()
        if new_hash == local_hash:
            print("%s exists; skipping" % local_filename)
            return True
    with open(local_filename, "w") as local_file:
        local_file.write(new_json)
        print("Wrote %s" % local_filename)
    return True

if __name__ == "__main__":
    local_cache_dir = "../../build/samples/shadertoy/cache"
    # TODO(cort): argparse
    shader_id = sys.argv[1]
    api_key = sys.argv[2]
    shader_info = fetch_shader_info(shader_id, api_key)
    validate_shader_info(shader_info)
    if not fetch_shader_inputs(shader_info, local_cache_dir):
        sys.exit(1)
    if not write_shader_source(shader_info, local_cache_dir):
        sys.exit(2)
    if not write_shader_info(shader_info, local_cache_dir):
        sys.exit(3)
