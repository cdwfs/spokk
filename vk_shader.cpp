#include "vk_context.h"
#include "vk_shader.h"

#include <vulkan/vulkan.h>

#include <spirv_glsl.hpp>

#include <cassert>
#include <cstdint>

namespace {

template<typename T>
T my_min(T x, T y) {
  return (x < y) ? x : y;
}
template<typename T>
T my_max(T x, T y) {
  return (x > y) ? x : y;
}

}  // namespace

namespace spokk {

#if defined(SPOKK_ENABLE_SHADERC)
//
// ShaderCompiler
//
ShaderCompiler::ShaderCompiler() : compiler_() {
}
ShaderCompiler::~ShaderCompiler() {
}
shaderc::SpvCompilationResult ShaderCompiler::CompileGlslString(const char *glsl_source,
    const std::string& logging_name, const std::string& entry_point,
    VkShaderStageFlagBits target_stage, const shaderc::CompileOptions& options) const {
  shaderc_shader_kind shader_kind = shaderc_glsl_infer_from_source; // VK_SHADER_STAGE_ALL = infer
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
    return shaderc::SpvCompilationResult(nullptr); // invalid shader stage
  }

  shaderc::SpvCompilationResult compile_result =
    compiler_.CompileGlslToSpv(glsl_source, shader_kind, logging_name.c_str(), entry_point.c_str(), options);
  return compile_result;
}
shaderc::SpvCompilationResult ShaderCompiler::CompileGlslFp(FILE *fp, int len_bytes,
    const std::string& logging_name, const std::string& entry_point,
    VkShaderStageFlagBits target_stage, const shaderc::CompileOptions& options) const {
  std::vector<char> glsl_str(len_bytes+1);
  size_t bytes_read = fread(glsl_str.data(), 1, len_bytes, fp);
  if ( (int)bytes_read != len_bytes) {
    return shaderc::SpvCompilationResult(nullptr);
  }
  glsl_str[len_bytes] = 0;
  return CompileGlslString(glsl_str.data(), logging_name, entry_point, target_stage, options);
}
shaderc::SpvCompilationResult ShaderCompiler::CompileGlslFile(const std::string& filename,
    const std::string& entry_point, VkShaderStageFlagBits target_stage, const shaderc::CompileOptions& options) const {
  FILE *glsl_file = fopen(filename.c_str(), "rb");
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


// Helper for SPIRV-Cross shader resource parsing
static void add_shader_resource_to_dset_layouts(std::vector<DescriptorSetLayoutInfo>& dset_layout_infos,
    const spirv_cross::CompilerGLSL& glsl, const spirv_cross::Resource& resource,
    VkDescriptorType desc_type, VkShaderStageFlagBits stage) {
  uint32_t dset_index     = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
  uint32_t binding_index = glsl.get_decoration(resource.id, spv::DecorationBinding);
  auto resource_type = glsl.get_type(resource.type_id);
  // In some cases, we need to tweak the descriptor type based on the resource type.
  if (resource_type.basetype == spirv_cross::SPIRType::SampledImage && resource_type.image.dim == spv::DimBuffer) {
    desc_type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
  } else if (resource_type.basetype == spirv_cross::SPIRType::Image && resource_type.image.dim == spv::DimBuffer) {
    desc_type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
  }
  uint32_t array_size = 1;
  for(auto arr_size : resource_type.array) {
    array_size *= arr_size;
  }
  // Add new dset(s) if necessary
  if (dset_index >= dset_layout_infos.size()) {
    dset_layout_infos.resize(dset_index + 1);
  }
  DescriptorSetLayoutInfo& layout_info = dset_layout_infos[dset_index];
  // Is this binding already in use?
  bool found_binding = false;
  for(uint32_t iBinding=0; iBinding<layout_info.bindings.size(); ++iBinding) {
    auto& binding = layout_info.bindings[iBinding];
    if (binding.binding == binding_index) {
      assert(binding.descriptorType == desc_type);  // same binding appears twice with different types
      assert(binding.descriptorCount == array_size);  // same binding appears twice with different array sizes
      binding.stageFlags |= stage;
      auto& binding_info = layout_info.binding_infos[iBinding];
      binding_info.stage_names.push_back(std::make_tuple(stage, glsl.get_name(resource.id)));
      found_binding = true;
      break;
    }
  }
  if (!found_binding) {
    VkDescriptorSetLayoutBinding new_binding = {};
    new_binding.binding = binding_index;
    new_binding.descriptorType = desc_type;
    new_binding.descriptorCount = array_size;
    new_binding.stageFlags = stage;
    new_binding.pImmutableSamplers = nullptr;
    layout_info.bindings.push_back(new_binding);
    DescriptorSetLayoutBindingInfo new_binding_info = {};
    new_binding_info.stage_names.push_back(std::make_tuple(stage, glsl.get_name(resource.id)));
    layout_info.binding_infos.push_back(new_binding_info);
  }
}

static void parse_shader_resources(std::vector<DescriptorSetLayoutInfo>& dset_layout_infos,
    VkPushConstantRange& push_constant_range, const spirv_cross::CompilerGLSL& glsl, VkShaderStageFlagBits stage) {
  spirv_cross::ShaderResources resources = glsl.get_shader_resources();
  // handle shader resources
  for (auto &resource : resources.uniform_buffers) {
    add_shader_resource_to_dset_layouts(dset_layout_infos, glsl, resource, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, stage);
  }
  for (auto &resource : resources.storage_buffers) {
    add_shader_resource_to_dset_layouts(dset_layout_infos, glsl, resource, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, stage);
  }
  for (auto &resource : resources.storage_images) {
    add_shader_resource_to_dset_layouts(dset_layout_infos, glsl, resource, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, stage);
  }
  for (auto &resource : resources.sampled_images) {
    add_shader_resource_to_dset_layouts(dset_layout_infos, glsl, resource, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, stage);
  }
  for (auto &resource : resources.separate_images) {
    add_shader_resource_to_dset_layouts(dset_layout_infos, glsl, resource, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, stage);
  }
  for (auto &resource : resources.separate_samplers) {
    add_shader_resource_to_dset_layouts(dset_layout_infos, glsl, resource, VK_DESCRIPTOR_TYPE_SAMPLER, stage);
  }
  for (auto &resource : resources.subpass_inputs) {
    add_shader_resource_to_dset_layouts(dset_layout_infos, glsl, resource, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, stage);
  }
  // Handle push constants. Each shader is only allowed to have one push constant block.
  push_constant_range = {};
  push_constant_range.stageFlags = stage;
  for (auto &resource : resources.push_constant_buffers) {
    size_t min_offset = UINT32_MAX, max_offset = 0;
    auto ranges = glsl.get_active_buffer_ranges(resource.id);
    if (!ranges.empty()) {
      for(const auto& range : ranges) {
        if (range.offset < min_offset)
          min_offset = range.offset;
        if (range.offset + range.range > max_offset)
          max_offset = range.offset + range.range;
      }
      push_constant_range.offset = (uint32_t)min_offset;
      push_constant_range.size = (uint32_t)(max_offset - min_offset);
    }
  }
#if 0
  // Handle stage inputs/outputs
  for (auto &resource : resources.stage_inputs) {
    uint32_t set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
    uint32_t binding = glsl.get_decoration(resource.id, spv::DecorationBinding);
    uint32_t loc = glsl.get_decoration(resource.id, spv::DecorationLocation);
    printf("set = %4u, binding = %4u, loc = %4u: stage input '%s'\n", set, binding, loc, resource.name.c_str());
  }
  for (auto &resource : resources.stage_outputs) {
    uint32_t set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
    uint32_t binding = glsl.get_decoration(resource.id, spv::DecorationBinding);
    uint32_t loc = glsl.get_decoration(resource.id, spv::DecorationLocation);
    printf("set = %4u, binding = %4u, loc = %4u: stage output '%s'\n", set, binding, loc, resource.name.c_str());
  }
  // ???
  for (auto &resource : resources.atomic_counters)
  {
    unsigned set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
    unsigned binding = glsl.get_decoration(resource.id, spv::DecorationBinding);
    printf("set = %4u, binding = %4u: atomic counter '%s'\n", set, binding, resource.name.c_str());
  }
#endif
}

//
// Shader
//
VkResult Shader::CreateAndLoadSpirvFile(const DeviceContext& device_context, const std::string& filename) {
  FILE *spv_file = fopen(filename.c_str(), "rb");
  if (!spv_file) {
    return VK_ERROR_INITIALIZATION_FAILED;
  }
  fseek(spv_file, 0, SEEK_END);
  long spv_file_size = ftell(spv_file);
  fseek(spv_file, 0, SEEK_SET);
  VkResult result = CreateAndLoadSpirvFp(device_context, spv_file, spv_file_size);
  fclose(spv_file);
  return result;
}
VkResult Shader::CreateAndLoadSpirvFp(const DeviceContext& device_context, FILE *fp, int len_bytes) {
  assert((len_bytes % sizeof(uint32_t)) == 0);
  spirv.resize(len_bytes/sizeof(uint32_t));
  size_t bytes_read = fread(spirv.data(), 1, len_bytes, fp);
  if ( (int)bytes_read != len_bytes) {
    return VK_ERROR_INITIALIZATION_FAILED;
  }
  return ParseSpirvAndCreate(device_context);
}
VkResult Shader::CreateAndLoadSpirvMem(const DeviceContext& device_context, const void *buffer, int len_bytes) {
  assert((len_bytes % sizeof(uint32_t)) == 0);
  spirv.reserve(len_bytes/sizeof(uint32_t));
  const uint32_t *buffer_as_u32 = (const uint32_t*)buffer;
  spirv.insert(spirv.begin(), buffer_as_u32, buffer_as_u32+(len_bytes/sizeof(uint32_t)));

  return ParseSpirvAndCreate(device_context);
}

VkResult Shader::ParseSpirvAndCreate(const DeviceContext& device_context) {
  spirv_cross::CompilerGLSL glsl(spirv);  // NOTE: throws an exception if you hand it malformed/invalid SPIRV.
  stage = VkShaderStageFlagBits(0);
  spv::ExecutionModel execution_model = glsl.get_execution_model();
  if        (execution_model == spv::ExecutionModelVertex) {
    stage = VK_SHADER_STAGE_VERTEX_BIT;
  } else if (execution_model == spv::ExecutionModelTessellationControl) {
    stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
  } else if (execution_model == spv::ExecutionModelTessellationEvaluation) {
    stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
  } else if (execution_model == spv::ExecutionModelGeometry) {
    stage = VK_SHADER_STAGE_GEOMETRY_BIT;
  } else if (execution_model == spv::ExecutionModelFragment) {
    stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  } else if (execution_model == spv::ExecutionModelGLCompute) {
    stage = VK_SHADER_STAGE_COMPUTE_BIT;
  }
  assert(stage != 0);

  parse_shader_resources(dset_layout_infos, push_constant_range, glsl, stage);

  VkShaderModuleCreateInfo shader_ci = {};
  shader_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  shader_ci.codeSize = spirv.size() * sizeof(uint32_t);  // note: in bytes
  shader_ci.pCode = spirv.data();
  VkResult result = vkCreateShaderModule(device_context.Device(), &shader_ci, device_context.HostAllocator(), &handle);
  return result;
}
void Shader::OverrideDescriptorType(uint32_t dset, uint32_t binding, VkDescriptorType new_type) {
  auto& b = dset_layout_infos.at(dset).bindings.at(binding);
  if (b.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER && new_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) {
    b.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
  } else if (b.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC && new_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
    b.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  } else if (b.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER && new_type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC) {
    b.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
  } else if (b.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC && new_type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
    b.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  }
}
void Shader::Destroy(const DeviceContext& device_context) {
  if (handle) {
    vkDestroyShaderModule(device_context.Device(), handle, device_context.HostAllocator());
    handle = VK_NULL_HANDLE;
  }
  dset_layout_infos.clear();
  UnloadSpirv();
  stage = (VkShaderStageFlagBits)0;
}

//
// ShaderPipeline
//
VkResult ShaderPipeline::AddShader(const Shader *shader, const char *entry_point) {
  if (pipeline_layout != VK_NULL_HANDLE) {
    return VK_ERROR_INITIALIZATION_FAILED; // pipeline is already finalized; can't add more shaders
  }
  if (shader == nullptr) {
    return VK_ERROR_INITIALIZATION_FAILED;
  }
  // check for another shader bound to this stage
  bool is_duplicate_stage = false;
  for(const auto &stage_ci : shader_stage_cis) {
    if (stage_ci.stage == shader->stage) {
      is_duplicate_stage = true;
      break;
    }
  }
  if (is_duplicate_stage) {
    return VK_ERROR_INITIALIZATION_FAILED;
  }
  // Add shader stage info
  size_t index = entry_point_names.size();
  assert(index == shader_stage_cis.size());
  entry_point_names.push_back(entry_point);
  VkPipelineShaderStageCreateInfo new_stage_ci = {};
  new_stage_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  new_stage_ci.stage = shader->stage;
  new_stage_ci.module = shader->handle;
  new_stage_ci.pName = nullptr; // set this in finalize() to avoid stale pointers.
  new_stage_ci.pSpecializationInfo = nullptr;  // TODO(cort): fill in at VkPipeline creation time
  shader_stage_cis.push_back(new_stage_ci);
  // Merge shader bindings with existing pipeline bindings
  // Grow descriptor set layout array if needed
  if (shader->dset_layout_infos.size() > dset_layout_infos.size()) {
    dset_layout_infos.resize(shader->dset_layout_infos.size());
  }
  // Add push constant range
  if (shader->push_constant_range.size > 0) {
    push_constant_ranges.push_back(shader->push_constant_range);
  }
  // Merge descriptor set layouts
  for(size_t iDS = 0; iDS < shader->dset_layout_infos.size(); ++iDS) {
    const DescriptorSetLayoutInfo& src_dset_layout_info = shader->dset_layout_infos[iDS];
    assert(src_dset_layout_info.bindings.size() == src_dset_layout_info.binding_infos.size());
    DescriptorSetLayoutInfo& dst_dset_layout_info = dset_layout_infos[iDS];
    for(size_t iSB = 0; iSB < src_dset_layout_info.bindings.size(); ++iSB) {
      const auto& src_binding = src_dset_layout_info.bindings[iSB];
      const auto& src_binding_info = src_dset_layout_info.binding_infos[iSB];
      assert(src_binding_info.stage_names.size() == 1);  // bindings in the source shader should only know about one stage, right?
      bool found_binding = false;
      for(size_t iDB = 0; iDB < dst_dset_layout_info.bindings.size(); ++iDB) {
        auto& dst_binding = dst_dset_layout_info.bindings[iDB];
        auto& dst_binding_info = dst_dset_layout_info.binding_infos[iDB];
        if (src_binding.binding == dst_binding.binding) {
          // TODO(cort): these asserts may not be valid; it may be possible for types/counts to differ in compatible ways
          assert(src_binding.descriptorType == dst_binding.descriptorType);  // same binding used with different types in two stages
          assert(src_binding.descriptorCount == dst_binding.descriptorCount);  // same binding used with different array sizes in two stages
                                                                               // Found a match!
          assert(0 == (dst_binding.stageFlags & shader->stage));  // Of course we haven't processed this stage yet, right?
          dst_binding.stageFlags |= src_binding.stageFlags;
          dst_binding_info.stage_names.push_back(src_binding_info.stage_names[0]);
          found_binding = true;
          break;
        }
      }
      if (!found_binding) {
        DescriptorSetLayoutBindingInfo new_binding_info = {};
        new_binding_info.stage_names = src_binding_info.stage_names;
        dst_dset_layout_info.bindings.push_back(src_binding);
        dst_dset_layout_info.binding_infos.push_back(new_binding_info);
      }
    }
  }
  return VK_SUCCESS;
}

VkResult ShaderPipeline::ForceCompatibleLayoutsAndFinalize(const DeviceContext& device_context,
  const std::vector<ShaderPipeline*> pipelines) {
  if (pipelines.empty()) {
    return VK_SUCCESS;
  }
  for(auto pipeline : pipelines) {
    if (pipeline == nullptr) {
      return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (pipeline->pipeline_layout != VK_NULL_HANDLE) {
      return VK_ERROR_INITIALIZATION_FAILED; // pipeline is already finalized; can't add more shaders
    }
  }
  // Merge pipelines 1..N into pipeline 0, then copy pipeline 0's layouts to 1..N.
  ShaderPipeline &dst_pipeline = *pipelines[0];
  for(size_t iPipeline = 1; iPipeline < pipelines.size(); ++iPipeline) {
    const ShaderPipeline &src_pipeline = *pipelines[iPipeline];
    // Merge shader bindings with existing pipeline bindings
    // Grow descriptor set layout array if needed
    if (src_pipeline.dset_layout_infos.size() > dst_pipeline.dset_layout_infos.size()) {
      dst_pipeline.dset_layout_infos.resize(src_pipeline.dset_layout_infos.size());
    }
    // Add push constant range. We'll merge them later.
    dst_pipeline.push_constant_ranges.insert(dst_pipeline.push_constant_ranges.end(),
      src_pipeline.push_constant_ranges.begin(), src_pipeline.push_constant_ranges.end());
    // Merge descriptor set layouts
    for(size_t iDS = 0; iDS < src_pipeline.dset_layout_infos.size(); ++iDS) {
      const DescriptorSetLayoutInfo& src_dset_layout_info = src_pipeline.dset_layout_infos[iDS];
      assert(src_dset_layout_info.bindings.size() == src_dset_layout_info.binding_infos.size());
      DescriptorSetLayoutInfo& dst_dset_layout_info = dst_pipeline.dset_layout_infos[iDS];
      for(size_t iSB = 0; iSB < src_dset_layout_info.bindings.size(); ++iSB) {
        const auto& src_binding = src_dset_layout_info.bindings[iSB];
        const auto& src_binding_info = src_dset_layout_info.binding_infos[iSB];
        bool found_binding = false;
        for(size_t iDB = 0; iDB < dst_dset_layout_info.bindings.size(); ++iDB) {
          auto& dst_binding = dst_dset_layout_info.bindings[iDB];
          auto& dst_binding_info = dst_dset_layout_info.binding_infos[iDB];
          if (src_binding.binding == dst_binding.binding) {
            // TODO(cort): these asserts may not be valid; it may be possible for types/counts to differ in compatible ways
            assert(src_binding.descriptorType == dst_binding.descriptorType);  // same binding used with different types in two stages
            assert(src_binding.descriptorCount == dst_binding.descriptorCount);  // same binding used with different array sizes in two stages
                                                                                 // Found a match!
            dst_binding.stageFlags |= src_binding.stageFlags;
            dst_binding_info.stage_names.push_back(src_binding_info.stage_names[0]);  // TODO(cort): this may invalidate the whole idea of saving these names; they can vary across pipelines.
            found_binding = true;
            break;
          }
        }
        if (!found_binding) {
          DescriptorSetLayoutBindingInfo new_binding_info = {};
          new_binding_info.stage_names = src_binding_info.stage_names;
          dst_dset_layout_info.bindings.push_back(src_binding);
          dst_dset_layout_info.binding_infos.push_back(new_binding_info);
        }
      }
    }
  }

  // Merge all the push constants into the minimal representation of the same ranges.
  // For now, we just union everything together into a single range. It's possible to be more conservative
  // and produce separate ranges for each shader stage if necessary, but I'm not sure that's necessary yet.
  if (!pipelines[0]->push_constant_ranges.empty()) {
    bool valid_range = false;
    std::vector<VkPushConstantRange> merged_range = {
      {VkShaderStageFlags(0), 0, 0} 
    };
    uint32_t merged_range_end = 0;
    for(const auto& range : pipelines[0]->push_constant_ranges) {
      if (range.size != 0) {
        if (!valid_range) {
          merged_range[0] = range;
          merged_range_end = merged_range[0].offset + merged_range[0].size;
          valid_range = true;
        } else {
          merged_range[0].offset = my_min(merged_range[0].offset, range.offset);
          merged_range_end = my_max(merged_range_end, range.offset+range.size);
          merged_range[0].stageFlags |= range.stageFlags;
        }
      }
    }
    if (valid_range) {
      merged_range[0].size = merged_range_end - merged_range[0].offset;
    }
    pipelines[0]->push_constant_ranges.swap(merged_range);
  }

  // Broadcast final merged layouts back to remaining pipelines
  for(size_t iPipeline = 1; iPipeline < pipelines.size(); ++iPipeline) {
    pipelines[iPipeline]->dset_layout_infos = pipelines[0]->dset_layout_infos;
    pipelines[iPipeline]->push_constant_ranges = pipelines[0]->push_constant_ranges;
  }

  for(auto pipeline : pipelines) {
    pipeline->Finalize(device_context);
  }
  return VK_SUCCESS;
}

VkResult ShaderPipeline::Finalize(const DeviceContext& device_context) {
  // Determine active shader stages
  active_stages = 0;
  for(const auto& stage_ci : shader_stage_cis) {
    if (active_stages & stage_ci.stage) {
      return VK_ERROR_INITIALIZATION_FAILED;  // Duplicate shader stage
    }
    active_stages |= stage_ci.stage;
  }
  constexpr std::array<VkShaderStageFlags, 5> valid_stage_combos = {{
      VK_SHADER_STAGE_COMPUTE_BIT,
      VK_SHADER_STAGE_VERTEX_BIT,
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT,
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      // TODO(cort): tessellation? Or, hey, not.
    }};
  bool stages_are_valid = false;
  for(auto combo : valid_stage_combos) {
    if (active_stages == combo) {
      stages_are_valid = true;
      break;
    }
  }
  if (!stages_are_valid) {
    active_stages = 0;
    return VK_ERROR_INITIALIZATION_FAILED;  // Invalid combination of shader stages
  }

  // Create the descriptor set layouts, now that their contents are known
  dset_layout_cis.resize(dset_layout_infos.size());
  dset_layouts.resize(dset_layout_infos.size());
  for(uint32_t iLayout = 0; iLayout < dset_layouts.size(); ++iLayout) {
    VkDescriptorSetLayoutCreateInfo& layout_ci = dset_layout_cis[iLayout];
    const DescriptorSetLayoutInfo& layout_info = dset_layout_infos[iLayout];
    assert(layout_info.bindings.size() == layout_info.binding_infos.size());
    layout_ci = {};
    layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_ci.bindingCount = (uint32_t)layout_info.bindings.size();
    layout_ci.pBindings = layout_info.bindings.data();
    VkResult result = vkCreateDescriptorSetLayout(device_context.Device(),
      &layout_ci, device_context.HostAllocator(), &dset_layouts[iLayout]);
    if (result != VK_SUCCESS) {
      return result;
    }
  }
  // Create the pipeline layout
  VkPipelineLayoutCreateInfo pipeline_layout_ci = {};
  pipeline_layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_ci.setLayoutCount = (uint32_t)dset_layouts.size();
  pipeline_layout_ci.pSetLayouts = dset_layouts.data();
  pipeline_layout_ci.pushConstantRangeCount = (uint32_t)push_constant_ranges.size();
  pipeline_layout_ci.pPushConstantRanges = push_constant_ranges.data();
  VkResult result = vkCreatePipelineLayout(device_context.Device(), &pipeline_layout_ci,
    device_context.HostAllocator(), &pipeline_layout);
  // Set entry point names now that the shader count is finalized
  for(size_t iShader=0; iShader<shader_stage_cis.size(); ++iShader) {
    shader_stage_cis[iShader].pName = entry_point_names[iShader].c_str();
  }

  return result;
}
void ShaderPipeline::Destroy(const DeviceContext& device_context) {
  for(auto dset_layout : dset_layouts) {
    if (dset_layout != VK_NULL_HANDLE) {
      vkDestroyDescriptorSetLayout(device_context.Device(), dset_layout, device_context.HostAllocator());
    }
  }
  dset_layouts.clear();
  dset_layout_cis.clear();
  dset_layout_infos.clear();
  push_constant_ranges.clear();
  shader_stage_cis.clear();
  entry_point_names.clear();
  vkDestroyPipelineLayout(device_context.Device(), pipeline_layout, device_context.HostAllocator());
  pipeline_layout = VK_NULL_HANDLE;
  active_stages = 0;
}

//
// DescriptorPool
//
DescriptorPool::DescriptorPool() : handle(VK_NULL_HANDLE), ci{}, pool_sizes{} {
  ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  ci.flags = 0;  // overriden in finalize()
  ci.poolSizeCount = (uint32_t)pool_sizes.size();
  ci.pPoolSizes = pool_sizes.data();
  for(size_t i = 0; i < VK_DESCRIPTOR_TYPE_RANGE_SIZE; ++i) {
    pool_sizes[i].type = (VkDescriptorType)i;
    pool_sizes[i].descriptorCount = 0;
  }
}
void DescriptorPool::Add(uint32_t layout_count, const VkDescriptorSetLayoutCreateInfo* dset_layout_cis, const uint32_t* dsets_per_layout) {
  for(uint32_t iLayout = 0; iLayout < layout_count; ++iLayout) {
    const VkDescriptorSetLayoutCreateInfo &layout = dset_layout_cis[iLayout];
    uint32_t dset_count = (dsets_per_layout != nullptr) ? dsets_per_layout[iLayout] : 1;
    Add(layout, dset_count);
  }
}
void DescriptorPool::Add(const VkDescriptorSetLayoutCreateInfo& dset_layout, uint32_t dset_count) {
  for(uint32_t iBinding = 0; iBinding < dset_layout.bindingCount; ++iBinding) {
    const VkDescriptorSetLayoutBinding &binding = dset_layout.pBindings[iBinding];
    pool_sizes[binding.descriptorType].descriptorCount += binding.descriptorCount * dset_count;
  }
  ci.maxSets += dset_count;
}
VkResult DescriptorPool::Finalize(const DeviceContext& device_context, VkDescriptorPoolCreateFlags flags) {
  ci.flags = flags;
  VkResult result = vkCreateDescriptorPool(device_context.Device(), &ci, device_context.HostAllocator(), &handle);
  return result;
}
void DescriptorPool::Destroy(const DeviceContext& device_context) {
  if (handle != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(device_context.Device(), handle, device_context.HostAllocator());
    handle = VK_NULL_HANDLE;
  }
}
VkResult DescriptorPool::AllocateSets(const DeviceContext& device_context, uint32_t dset_count,
  const VkDescriptorSetLayout *dset_layouts, VkDescriptorSet *out_dsets) const {
  VkDescriptorSetAllocateInfo alloc_info = {};
  alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info.descriptorPool = handle;
  alloc_info.descriptorSetCount = dset_count;
  alloc_info.pSetLayouts = dset_layouts;
  VkResult result = vkAllocateDescriptorSets(device_context.Device(), &alloc_info, out_dsets);
  return result;
}
VkDescriptorSet DescriptorPool::AllocateSet(const DeviceContext& device_context,
  VkDescriptorSetLayout dset_layout) const {
  VkDescriptorSet dset = VK_NULL_HANDLE;
  AllocateSets(device_context, 1, &dset_layout, &dset);
  return dset;
}
void DescriptorPool::FreeSets(DeviceContext& device_context, uint32_t set_count, const VkDescriptorSet* sets) const {
  if (ci.flags & VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT) {
    vkFreeDescriptorSets(device_context.Device(), handle, set_count, sets);
  }
}
void DescriptorPool::FreeSet(DeviceContext& device_context, VkDescriptorSet set) const {
  if (ci.flags & VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT) {
    vkFreeDescriptorSets(device_context.Device(), handle, 1, &set);
  }
}

//
// DescriptorSetWriter
//
DescriptorSetWriter::DescriptorSetWriter(const VkDescriptorSetLayoutCreateInfo &layout_ci) :
    image_infos{}, buffer_infos{}, texel_buffer_views{}, binding_writes{layout_ci.bindingCount} {
  // First time through, we're building a total count of each class of descriptor.
  uint32_t image_count = 0, buffer_count = 0, texel_buffer_count = 0;
  for(uint32_t iBinding = 0; iBinding < layout_ci.bindingCount; ++iBinding) {
    const VkDescriptorSetLayoutBinding &binding = layout_ci.pBindings[iBinding];
    if (binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER ||
        binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER) {
      texel_buffer_count += binding.descriptorCount;
    } else if (binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
        binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
        binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
        binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC) {
      buffer_count += binding.descriptorCount;
    } else if (binding.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
        binding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
        binding.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
        binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
        binding.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT) {
      image_count += binding.descriptorCount;
    }
  }
  image_infos.resize(image_count);
  buffer_infos.resize(buffer_count);
  texel_buffer_views.resize(texel_buffer_count);

  // Now we can populate the Writes, and point each Write to the appropriate info(s).
  uint32_t next_image_info = 0, next_buffer_info = 0, next_texel_buffer_view = 0;
  for(uint32_t iBinding = 0; iBinding < layout_ci.bindingCount; ++iBinding) {
    const VkDescriptorSetLayoutBinding &binding = layout_ci.pBindings[iBinding];
    VkWriteDescriptorSet& write = binding_writes[iBinding];
    write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = VK_NULL_HANDLE;  // filled in just-in-time when writing
    write.dstBinding = binding.binding;
    write.dstArrayElement = 0;
    write.descriptorCount = binding.descriptorCount;
    write.descriptorType = binding.descriptorType;
    if (binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER ||
        binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER) {
      write.pTexelBufferView = &texel_buffer_views[next_texel_buffer_view];
      next_texel_buffer_view += binding.descriptorCount;
    } else if (binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
        binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
        binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
        binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC) {
      write.pBufferInfo = &buffer_infos[next_buffer_info];
      next_buffer_info += binding.descriptorCount;
    } else if (binding.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
        binding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
        binding.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
        binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
        binding.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT) {
      write.pImageInfo = &image_infos[next_image_info];
      next_image_info += write.descriptorCount;
    }
  }
  assert(next_texel_buffer_view == texel_buffer_views.size());
  assert(next_buffer_info == buffer_infos.size());
  assert(next_image_info == image_infos.size());
}
void DescriptorSetWriter::BindCombinedImageSampler(VkImageView view, VkImageLayout layout, VkSampler sampler, uint32_t binding, uint32_t array_element) {
  // TODO(cort): make this search more efficient!
  VkWriteDescriptorSet *write = nullptr;
  for(size_t iWrite=0; iWrite<binding_writes.size(); ++iWrite) {
    if (binding_writes[iWrite].dstBinding == binding) {
      write = &binding_writes[iWrite];
      break;
    }
  }
  assert(write != nullptr);
  assert(array_element < write->descriptorCount);
  assert(write->pImageInfo != nullptr); // is this binding really an image-type descriptor?
  auto *pImageInfo = const_cast<VkDescriptorImageInfo*>(write->pImageInfo);
  pImageInfo->imageView = view;
  pImageInfo->imageLayout = layout;
  pImageInfo->sampler = sampler;
}
void DescriptorSetWriter::BindBuffer(VkBuffer buffer, uint32_t binding, VkDeviceSize offset, VkDeviceSize range, uint32_t array_element) {
  // TODO(cort): make this search more efficient!
  VkWriteDescriptorSet *write = nullptr;
  for(size_t iWrite=0; iWrite<binding_writes.size(); ++iWrite) {
    if (binding_writes[iWrite].dstBinding == binding) {
      write = &binding_writes[iWrite];
      break;
    }
  }
  assert(write != nullptr);
  assert(array_element < write->descriptorCount);
  assert(write->pBufferInfo != nullptr); // is this binding really a buffer-type descriptor?
  auto *pBufferInfo = const_cast<VkDescriptorBufferInfo*>(write->pBufferInfo);
  pBufferInfo->buffer = buffer;
  pBufferInfo->offset = offset;
  pBufferInfo->range = range;
}
void DescriptorSetWriter::BindTexelBuffer(VkBufferView view, uint32_t binding, uint32_t array_element) {
  // TODO(cort): make this search more efficient!
  VkWriteDescriptorSet *write = nullptr;
  for(size_t iWrite=0; iWrite<binding_writes.size(); ++iWrite) {
    if (binding_writes[iWrite].dstBinding == binding) {
      write = &binding_writes[iWrite];
      break;
    }
  }
  assert(write != nullptr);
  assert(array_element < write->descriptorCount);
  assert(write->pTexelBufferView != nullptr); // is this binding really a texel buffer descriptor?
  auto *pTexelBufferView = const_cast<VkBufferView*>(write->pTexelBufferView);
  *pTexelBufferView = view;
}
void DescriptorSetWriter::WriteAll(const DeviceContext& device_context, VkDescriptorSet dest_set) {
  for(auto& write : binding_writes) {
    write.dstSet = dest_set;
  }
  vkUpdateDescriptorSets(device_context.Device(),
    (uint32_t)binding_writes.size(), binding_writes.data(), 0, nullptr);
}
void DescriptorSetWriter::WriteOne(const DeviceContext& device_context, VkDescriptorSet dest_set,
    uint32_t binding, uint32_t array_element) {
  assert(binding < binding_writes.size());
  VkWriteDescriptorSet writeCopy = binding_writes[binding];
  assert(array_element < writeCopy.dstArrayElement);
  writeCopy.dstSet = dest_set;
  writeCopy.dstArrayElement = array_element;
  if (writeCopy.pImageInfo != nullptr) {
    writeCopy.pImageInfo += array_element;
  } else if (writeCopy.pBufferInfo != nullptr) {
    writeCopy.pBufferInfo += array_element;
  } else if (writeCopy.pTexelBufferView != nullptr) {
    writeCopy.pTexelBufferView += array_element;
  }
  vkUpdateDescriptorSets(device_context.Device(), 1, &writeCopy, 0, nullptr);
}

}  // namespace spokk
