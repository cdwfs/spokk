spokk
=====

Just a framework for building simple [Vulkan](https://www.khronos.org/vulkan/) applications.
![image](https://raw.githubusercontent.com/cdwfs/spokk/master/samples/cubeswarm/screenshot.jpg)
![image](https://raw.githubusercontent.com/cdwfs/spokk/master/samples/pillars/screenshot.jpg)

Acknowledgements
----------------
spokk builds upon the following projects, which will be automatically included and configured as submodules):
- [assimp/assimp](https://github.com/assimp/assimp) for loading common 3D file formats.
- [glfw/glfw](https://github.com/glfw/glfw) for windowing and keyboard/mouse input.
- [sheredom/json.h](https://github.com/sheredom/json.h) for JSON parsing.
- [google/mathfu](https://github.com/google/mathfu) for 3D math.
- [google/shaderc](https://github.com/google/shaderc) for runtime shader compilation.
- [KhronosGroup/SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross) for introspection of
  SPIR-V shaders (used to automatically generate descriptor set layouts).
- [nothings/stb_image](https://github.com/nothings/stb) for loading common image formats.
