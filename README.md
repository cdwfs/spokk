spokk
=====

Just a framework for building simple [Vulkan](https://www.khronos.org/vulkan/) applications.
![image](https://raw.githubusercontent.com/cdwfs/spokk/master/samples/cubeswarm/screenshot.jpg)
![image](https://raw.githubusercontent.com/cdwfs/spokk/master/samples/lights/screenshot.jpg)
![image](https://raw.githubusercontent.com/cdwfs/spokk/master/samples/pillars/screenshot.jpg)

How To Build
------------
All external dependencies are configured as submodules; after cloning the spokk repo, run the
following commands to clone and sync the appropriate revisions:

```
$ git submodule init
$ git submodule update
```

Next, use CMake in the traditional platform-appropriate fashion to generate the project files of
your choice. Build and run any of the "samples" projects.

Acknowledgements
----------------
spokk builds upon the following projects, which will be automatically included and configured as submodules):
- [assimp/assimp](https://github.com/assimp/assimp) for loading common 3D file formats.
- [glfw/glfw](https://github.com/glfw/glfw) for windowing and keyboard/mouse input.
- [sheredom/json.h](https://github.com/sheredom/json.h) for JSON parsing.
- [g-truc/glm](https://github.com/google/mathfu) for 3D math.
- [google/shaderc](https://github.com/google/shaderc) for shader compilation.
- [ocornut/imgui](https://github.com/ocornut/imgui) for quick & dirty runtime GUI.
- [KhronosGroup/SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross) for introspection of
  SPIR-V shaders (used to automatically generate descriptor set layouts).
- [nothings/stb](https://github.com/nothings/stb) for loading common image formats (`stb_image`), writing
  images (`stb_image_write`), mipmap generation (`stb_image_resize`), and TrueType font loading
  (`stb_truetype`).
- [GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)
  for Vulkan device memory allocation.
