#pragma once

#include "spokk_device.h"
#include "spokk_shader.h"

#include <shaderc/shaderc.hpp>

#include <stdio.h>
#include <string>

/**
 * To use this class:
 * - Declare a ShaderCompiler object:
 *     ShaderCompiler compiler;
 * - [optional] Declare and configure a shaderc::CompileOptions object. Default options
 *   can be selected by omitting this parameter during shader compilation.
 *     shaderc::CompileOptions compile_options;
 *     compile_options.AddMacroDefinition(...);
 * - Compile the shader to SPIR-V, capturing the compilation results. Different calls are provided
 *   depending on whether the shader code is sourced from memory, a FILE pointer, or a filename on disk.
 *   The entry point is assumed to be "main" if the parameter is omitted.
 *   If the target_stage parameter is VK_SHADER_STAGE_ALL, the function will attempt to infer the shader
 *   stage from the source code (e.g. using the #pragma shader_stage() info, if provided).
 *     shaderc::SpvCompilationResult compile_result = compiler.CompileGlslFile("shader.vert",
 *         "main", VK_SHADER_STAGE_VERTEX, compile_options);
 * - If the compilation was successful, load the resulting SPIR-V into a spokk::Shader object.
 *     if (compile_result.GetCompilationStatus() != shaderc_compilation_status_success) {
 *       // GLSL -> SPIR-V compilation failed!
 *     }
 *     spokk::Shader shader;
 *     VkResult create_result = shader.CreateAndLoadSpirvMem(device, compile_result.cbegin(),
 *         static_cast<int>((compile_result.cend() - compile_result.cbegin()) * sizeof(uint32_t)));
 *     if (create_result != VK_SUCCESS) {
 *       // SPIR-V -> VkShaderModule creation failed!
 *     }
 */

class ShaderCompiler {
public:
  ShaderCompiler();
  ~ShaderCompiler();

  // GLSL compilation functions
  shaderc::SpvCompilationResult CompileGlslString(const char* glsl_source, const std::string& logging_name,
      const std::string& entry_point = "main", VkShaderStageFlagBits target_stage = VK_SHADER_STAGE_ALL,
      const shaderc::CompileOptions& options = shaderc::CompileOptions()) const;
  shaderc::SpvCompilationResult CompileGlslFp(FILE* fp, int len_bytes, const std::string& logging_name,
      const std::string& entry_point = "main", VkShaderStageFlagBits target_stage = VK_SHADER_STAGE_ALL,
      const shaderc::CompileOptions& options = shaderc::CompileOptions()) const;
  shaderc::SpvCompilationResult CompileGlslFile(const std::string& filename, const std::string& entry_point = "main",
      VkShaderStageFlagBits target_stage = VK_SHADER_STAGE_ALL,
      const shaderc::CompileOptions& = shaderc::CompileOptions()) const;

  // HLSL compilation functions
  shaderc::SpvCompilationResult CompileHlslString(const char* hlsl_source, const std::string& logging_name,
      const std::string& entry_point = "main", VkShaderStageFlagBits target_stage = VK_SHADER_STAGE_ALL,
      const shaderc::CompileOptions& options = shaderc::CompileOptions()) const {
    shaderc::CompileOptions final_options(options);
    final_options.SetSourceLanguage(shaderc_source_language_hlsl);
    return CompileGlslString(hlsl_source, logging_name, entry_point, target_stage, final_options);
  }
  shaderc::SpvCompilationResult CompileHlslFp(FILE* fp, int len_bytes, const std::string& logging_name,
      const std::string& entry_point = "main", VkShaderStageFlagBits target_stage = VK_SHADER_STAGE_ALL,
      const shaderc::CompileOptions& options = shaderc::CompileOptions()) const {
    shaderc::CompileOptions final_options(options);
    final_options.SetSourceLanguage(shaderc_source_language_hlsl);
    return CompileGlslFp(fp, len_bytes, logging_name, entry_point, target_stage, final_options);
  }
  shaderc::SpvCompilationResult CompileHlslFile(const std::string& filename, const std::string& entry_point = "main",
      VkShaderStageFlagBits target_stage = VK_SHADER_STAGE_ALL,
      const shaderc::CompileOptions& options = shaderc::CompileOptions()) const {
    shaderc::CompileOptions final_options(options);
    final_options.SetSourceLanguage(shaderc_source_language_hlsl);
    return CompileGlslFile(filename, entry_point, target_stage, final_options);
  }

protected:
  shaderc::Compiler compiler_;
};
