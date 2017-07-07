#include "shader_compiler.h"

#if defined(SPOKK_ENABLE_SHADERC)

//
// ShaderCompiler
//
ShaderCompiler::ShaderCompiler() : compiler_() {}
ShaderCompiler::~ShaderCompiler() {}
shaderc::SpvCompilationResult ShaderCompiler::CompileGlslString(const char* glsl_source,
    const std::string& logging_name, const std::string& entry_point, VkShaderStageFlagBits target_stage,
    const shaderc::CompileOptions& options) const {
  shaderc_shader_kind shader_kind = shaderc_glsl_infer_from_source;  // VK_SHADER_STAGE_ALL = infer
  if (target_stage == VK_SHADER_STAGE_COMPUTE_BIT) {
    shader_kind = shaderc_glsl_default_compute_shader;
  } else if (target_stage == VK_SHADER_STAGE_VERTEX_BIT) {
    shader_kind = shaderc_glsl_default_vertex_shader;
  } else if (target_stage == VK_SHADER_STAGE_FRAGMENT_BIT) {
    shader_kind = shaderc_glsl_default_fragment_shader;
  } else if (target_stage == VK_SHADER_STAGE_GEOMETRY_BIT) {
    shader_kind = shaderc_glsl_default_geometry_shader;
  } else if (target_stage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) {
    shader_kind = shaderc_glsl_default_tess_control_shader;
  } else if (target_stage == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) {
    shader_kind = shaderc_glsl_default_tess_evaluation_shader;
  } else if (target_stage != VK_SHADER_STAGE_ALL) {
    return shaderc::SpvCompilationResult(nullptr);  // invalid shader stage
  }

  shaderc::SpvCompilationResult compile_result =
      compiler_.CompileGlslToSpv(glsl_source, shader_kind, logging_name.c_str(), entry_point.c_str(), options);
  return compile_result;
}
shaderc::SpvCompilationResult ShaderCompiler::CompileGlslFp(FILE* fp, int len_bytes, const std::string& logging_name,
    const std::string& entry_point, VkShaderStageFlagBits target_stage, const shaderc::CompileOptions& options) const {
  std::vector<char> glsl_str(len_bytes + 1);
  size_t bytes_read = fread(glsl_str.data(), 1, len_bytes, fp);
  if ((int)bytes_read != len_bytes) {
    return shaderc::SpvCompilationResult(nullptr);
  }
  glsl_str[len_bytes] = 0;
  return CompileGlslString(glsl_str.data(), logging_name, entry_point, target_stage, options);
}
shaderc::SpvCompilationResult ShaderCompiler::CompileGlslFile(const std::string& filename,
    const std::string& entry_point, VkShaderStageFlagBits target_stage, const shaderc::CompileOptions& options) const {
  FILE* glsl_file = fopen(filename.c_str(), "rb");
  if (!glsl_file) {
    return shaderc::SpvCompilationResult(nullptr);
  }
  fseek(glsl_file, 0, SEEK_END);
  long glsl_file_size = ftell(glsl_file);
  fseek(glsl_file, 0, SEEK_SET);
  auto result = CompileGlslFp(glsl_file, glsl_file_size, filename, entry_point, target_stage, options);
  fclose(glsl_file);
  return result;
}

#endif  // defined(SPOKK_ENABLE_SHADERC)
