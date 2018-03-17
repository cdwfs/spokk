#include "spokk_shader.h"
#include "spokk_platform.h"

#include <spirv_glsl.hpp>

#include <algorithm>
#include <cstdint>

namespace spokk {

// Helper for SPIRV-Cross shader resource parsing
void Shader::AddShaderResourceToDescriptorSetLayout(
    const spirv_cross::CompilerGLSL& glsl, const spirv_cross::Resource& resource, VkDescriptorType desc_type) {
  uint32_t dset_index = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
  uint32_t binding_index = glsl.get_decoration(resource.id, spv::DecorationBinding);
  auto resource_type = glsl.get_type(resource.type_id);
  // In some cases, we need to tweak the descriptor type based on the resource type.
  if (resource_type.basetype == spirv_cross::SPIRType::SampledImage && resource_type.image.dim == spv::DimBuffer) {
    desc_type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
  } else if (resource_type.basetype == spirv_cross::SPIRType::Image && resource_type.image.dim == spv::DimBuffer) {
    desc_type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
  }
  uint32_t array_size = 1;
  for (auto arr_size : resource_type.array) {
    array_size *= arr_size;
  }
  // Add new (empty) dset(s) if necessary
  if (dset_index >= dset_layout_infos.size()) {
    dset_layout_infos.resize(dset_index + 1);
  }
  DescriptorSetLayoutInfo& layout_info = dset_layout_infos[dset_index];
  // Is this binding already in use?
  bool found_binding = false;
  for (uint32_t iBinding = 0; iBinding < layout_info.bindings.size(); ++iBinding) {
    auto& binding = layout_info.bindings[iBinding];
    if (binding.binding == binding_index) {
      //ZOMBO_ERROR("Binding %u appears twice in a Shader? WTF?", binding_index);
      //ZOMBO_ASSERT(binding.descriptorType == desc_type, "binding %u appears twice with different types in shader",
      //    binding_index);
      ZOMBO_ASSERT(binding.descriptorCount == array_size,
          "binding %u appears twice with different array sizes in shader", binding_index);
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

    const std::string& binding_name = glsl.get_name(resource.id);
    ZOMBO_ASSERT(name_to_index_.find(binding_name) == name_to_index_.cend(),
        "Binding name '%s' appears multiple times in shader?", binding_name.c_str());
    DescriptorBindPoint bind_point = {};
    bind_point.set = dset_index;
    bind_point.binding = binding_index;
    name_to_index_[binding_name] = bind_point;
  }
}

void Shader::ParseShaderResources(const spirv_cross::CompilerGLSL& glsl) {
  spirv_cross::ShaderResources resources = glsl.get_shader_resources();
  // handle shader resources
  for (auto& resource : resources.uniform_buffers) {
    AddShaderResourceToDescriptorSetLayout(glsl, resource, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  }
  for (auto& resource : resources.storage_buffers) {
    AddShaderResourceToDescriptorSetLayout(glsl, resource, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
  }
  for (auto& resource : resources.storage_images) {
    AddShaderResourceToDescriptorSetLayout(glsl, resource, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
  }
  for (auto& resource : resources.sampled_images) {
    AddShaderResourceToDescriptorSetLayout(glsl, resource, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  }
  for (auto& resource : resources.separate_images) {
    AddShaderResourceToDescriptorSetLayout(glsl, resource, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
  }
  for (auto& resource : resources.separate_samplers) {
    AddShaderResourceToDescriptorSetLayout(glsl, resource, VK_DESCRIPTOR_TYPE_SAMPLER);
  }
  for (auto& resource : resources.subpass_inputs) {
    AddShaderResourceToDescriptorSetLayout(glsl, resource, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);
  }
  // Handle push constants. Each shader stage is only allowed to have one push constant block, so if the SPIRV
  // defines more than one range, we have to merge them here.
  push_constant_range = {};
  push_constant_range.stageFlags = stage;
  for (auto& resource : resources.push_constant_buffers) {
    size_t min_offset = SIZE_MAX, first_unused_offset = 0;
    auto ranges = glsl.get_active_buffer_ranges(resource.id);
    if (!ranges.empty()) {
      for (const auto& range : ranges) {
        if (range.offset < min_offset) {
          min_offset = range.offset;
        }
        if (range.offset + range.range > first_unused_offset) {
          first_unused_offset = range.offset + range.range;
        }
      }
      push_constant_range.offset = (uint32_t)min_offset;
      push_constant_range.size = (uint32_t)(first_unused_offset - min_offset);
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
VkResult Shader::CreateAndLoadSpirvFile(const Device& device, const std::string& filename) {
  FILE* spv_file = fopen(filename.c_str(), "rb");
  if (!spv_file) {
    return VK_ERROR_INITIALIZATION_FAILED;
  }
  fseek(spv_file, 0, SEEK_END);
  long spv_file_size = ftell(spv_file);
  fseek(spv_file, 0, SEEK_SET);
  VkResult result = CreateAndLoadSpirvFp(device, spv_file, spv_file_size);
  fclose(spv_file);
  return result;
}
VkResult Shader::CreateAndLoadSpirvFp(const Device& device, FILE* fp, int len_bytes) {
  ZOMBO_ASSERT_RETURN((len_bytes % sizeof(uint32_t)) == 0, VK_ERROR_INITIALIZATION_FAILED,
      "len_bytes (%d) must be divisible by 4", len_bytes);
  spirv.resize(len_bytes / sizeof(uint32_t));
  size_t bytes_read = fread(spirv.data(), 1, len_bytes, fp);
  if ((int)bytes_read != len_bytes) {
    return VK_ERROR_INITIALIZATION_FAILED;
  }
  return ParseSpirvAndCreate(device);
}
VkResult Shader::CreateAndLoadSpirvMem(const Device& device, const void* buffer, int len_bytes) {
  ZOMBO_ASSERT_RETURN((len_bytes % sizeof(uint32_t)) == 0, VK_ERROR_INITIALIZATION_FAILED,
      "len_bytes (%d) must be divisible by 4", len_bytes);
  spirv.reserve(len_bytes / sizeof(uint32_t));
  const uint32_t* buffer_as_u32 = (const uint32_t*)buffer;
  spirv.insert(spirv.begin(), buffer_as_u32, buffer_as_u32 + (len_bytes / sizeof(uint32_t)));

  return ParseSpirvAndCreate(device);
}

VkResult Shader::ParseSpirvAndCreate(const Device& device) {
  spirv_cross::CompilerGLSL glsl(spirv);  // NOTE: throws an exception if you hand it malformed/invalid SPIRV.
  stage = VkShaderStageFlagBits(0);
  spv::ExecutionModel execution_model = glsl.get_execution_model();
  if (execution_model == spv::ExecutionModelVertex) {
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
  ZOMBO_ASSERT_RETURN(stage != 0, VK_ERROR_INITIALIZATION_FAILED, "invalid shader stage %d", stage);

  ParseShaderResources(glsl);

  // validation
  for (size_t s = 0; s < dset_layout_infos.size(); ++s) {
    const auto& layout_info = dset_layout_infos[s];
    for (size_t b = 0; b < layout_info.bindings.size(); ++b) {
      // For a single shader, ensure that each binding's stage matches the stage of the shader itself
      ZOMBO_ASSERT_RETURN(0 != (layout_info.bindings[b].stageFlags & stage), VK_ERROR_INITIALIZATION_FAILED,
          "invariant failure: set %u binding %u binding stageFlags (%u) do not match shader stage (%u)", (uint32_t)s,
          (uint32_t)b, layout_info.bindings[b].stageFlags, stage);
    }
  }

  VkShaderModuleCreateInfo shader_ci = {};
  shader_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  shader_ci.codeSize = spirv.size() * sizeof(uint32_t);  // note: in bytes
  shader_ci.pCode = spirv.data();
  VkResult result = vkCreateShaderModule(device, &shader_ci, device.HostAllocator(), &handle);
  return result;
}
void Shader::OverrideDescriptorType(uint32_t dset, uint32_t binding, VkDescriptorType new_type) {
  auto& b = dset_layout_infos.at(dset).bindings.at(binding);
  if (b.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER && new_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) {
    b.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
  } else if (b.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC &&
      new_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
    b.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  } else if (b.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER &&
      new_type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC) {
    b.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
  } else if (b.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC &&
      new_type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
    b.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  }
}

DescriptorBindPoint Shader::GetDescriptorBindPoint(const std::string& name) const {
  const auto itor = name_to_index_.find(name);
  if (itor != name_to_index_.cend()) {
    return (*itor).second;
  }
  ZOMBO_ERROR("Desriptor '%s' not found in shader", name.c_str());
  return {UINT32_MAX, UINT32_MAX};  // TODO(cort): better "not found" error?
}

void Shader::Destroy(const Device& device) {
  if (handle) {
    vkDestroyShaderModule(device, handle, device.HostAllocator());
    handle = VK_NULL_HANDLE;
  }
  dset_layout_infos.clear();
  UnloadSpirv();
  stage = (VkShaderStageFlagBits)0;
}

//
// ShaderProgram
//
VkResult ShaderProgram::AddShader(const Shader* shader, const char* entry_point) {
  if (pipeline_layout != VK_NULL_HANDLE) {
    return VK_ERROR_INITIALIZATION_FAILED;  // program is already finalized; can't add more shaders
  }
  if (shader == nullptr) {
    return VK_ERROR_INITIALIZATION_FAILED;
  }
  // check for another shader bound to this stage
  bool is_duplicate_stage = false;
  for (const auto& stage_ci : shader_stage_cis) {
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
  ZOMBO_ASSERT(index == shader_stage_cis.size(), "invariant failure: shader stage array size mismatch");
  entry_point_names.push_back(entry_point);
  VkPipelineShaderStageCreateInfo new_stage_ci = {};
  new_stage_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  new_stage_ci.stage = shader->stage;
  new_stage_ci.module = shader->handle;
  new_stage_ci.pName = nullptr;  // set this in finalize() to avoid stale pointers.
  new_stage_ci.pSpecializationInfo = nullptr;  // Specialization constants are not currently supported.
  shader_stage_cis.push_back(new_stage_ci);

  int merge_error =
      MergeLayouts(shader->dset_layout_infos, std::vector<VkPushConstantRange>{shader->push_constant_range});
  if (merge_error) {
    return VK_ERROR_INITIALIZATION_FAILED;
  }

  return VK_SUCCESS;
}

VkResult ShaderProgram::ForceCompatibleLayoutsAndFinalize(
    const Device& device, const std::vector<ShaderProgram*> programs) {
  if (programs.empty()) {
    return VK_SUCCESS;
  }
  for (auto program : programs) {
    if (program == nullptr) {
      return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (program->pipeline_layout != VK_NULL_HANDLE) {
      return VK_ERROR_INITIALIZATION_FAILED;  // program is already finalized; can't add more shaders
    }
  }
  // Merge programs 1..N into program 0, then copy program 0's layouts to 1..N.
  // TODO(https://github.com/cdwfs/spokk/issues/1): extract into common function
  ShaderProgram& dst_program = *programs[0];
  for (size_t iProgram = 1; iProgram < programs.size(); ++iProgram) {
    const ShaderProgram& src_program = *programs[iProgram];
    int merge_error = dst_program.MergeLayouts(src_program.dset_layout_infos, src_program.push_constant_ranges);
    if (merge_error) {
      return VK_ERROR_INITIALIZATION_FAILED;
    }
  }

  // Broadcast final merged layouts back to remaining programs
  for (size_t iProgram = 1; iProgram < programs.size(); ++iProgram) {
    programs[iProgram]->dset_layout_infos = programs[0]->dset_layout_infos;
    programs[iProgram]->push_constant_ranges = programs[0]->push_constant_ranges;
  }

  for (auto program : programs) {
    program->Finalize(device);
  }
  return VK_SUCCESS;
}

VkResult ShaderProgram::Finalize(const Device& device) {
  // Determine active shader stages
  active_stages = 0;
  for (const auto& stage_ci : shader_stage_cis) {
    if (active_stages & stage_ci.stage) {
      return VK_ERROR_INITIALIZATION_FAILED;  // Duplicate shader stage
    }
    active_stages |= stage_ci.stage;
  }
  // clang-format off
  constexpr std::array<VkShaderStageFlags, 5> valid_stage_combos = {{
      VK_SHADER_STAGE_COMPUTE_BIT,
      VK_SHADER_STAGE_VERTEX_BIT,
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT,
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      // Look at me, refusing to acknowledge the existence of tessellation shaders.
  }};
  // clang-format on
  bool stages_are_valid = false;
  for (auto combo : valid_stage_combos) {
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
  dset_layouts.resize(dset_layout_infos.size(), VK_NULL_HANDLE);
  for (uint32_t iLayout = 0; iLayout < dset_layouts.size(); ++iLayout) {
    VkDescriptorSetLayoutCreateInfo& layout_ci = dset_layout_cis[iLayout];
    const DescriptorSetLayoutInfo& layout_info = dset_layout_infos[iLayout];
    layout_ci = {};
    layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_ci.bindingCount = (uint32_t)layout_info.bindings.size();
    layout_ci.pBindings = layout_info.bindings.data();
    // Arguably, we could skip creating empty dset layouts here. But the spec does imply that bindingCount=0
    // is valid, so for now I'm going to consider them harmless and leave them in.
    VkResult result = vkCreateDescriptorSetLayout(device, &layout_ci, device.HostAllocator(), &dset_layouts[iLayout]);
    if (result != VK_SUCCESS) {
      return result;
    }
  }
  // Create the pipeline layout
  VkPipelineLayoutCreateInfo pipeline_layout_ci = {};
  pipeline_layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_ci.setLayoutCount = (uint32_t)dset_layouts.size();
  pipeline_layout_ci.pSetLayouts = dset_layouts.data();
  ZOMBO_ASSERT(push_constant_ranges.size() == 1, "programs currently only support one push constant range");
  if (!push_constant_ranges.empty() && push_constant_ranges[0].size == 0) {
    pipeline_layout_ci.pushConstantRangeCount = 0;
  } else {
    pipeline_layout_ci.pushConstantRangeCount = (uint32_t)push_constant_ranges.size();
  }
  pipeline_layout_ci.pPushConstantRanges = push_constant_ranges.data();
  VkResult result = vkCreatePipelineLayout(device, &pipeline_layout_ci, device.HostAllocator(), &pipeline_layout);
  // Set entry point names now that the shader count is finalized
  for (size_t iShader = 0; iShader < shader_stage_cis.size(); ++iShader) {
    shader_stage_cis[iShader].pName = entry_point_names[iShader].c_str();
  }

  return result;
}
void ShaderProgram::Destroy(const Device& device) {
  for (auto dset_layout : dset_layouts) {
    if (dset_layout != VK_NULL_HANDLE) {
      vkDestroyDescriptorSetLayout(device, dset_layout, device.HostAllocator());
    }
  }
  dset_layouts.clear();
  dset_layout_cis.clear();
  dset_layout_infos.clear();
  push_constant_ranges.clear();
  shader_stage_cis.clear();
  entry_point_names.clear();
  if (pipeline_layout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(device, pipeline_layout, device.HostAllocator());
    pipeline_layout = VK_NULL_HANDLE;
  }
  active_stages = 0;
}

int ShaderProgram::MergeLayouts(const std::vector<DescriptorSetLayoutInfo>& new_dset_layout_infos,
    const std::vector<VkPushConstantRange> new_push_constant_ranges) {
  // Work on a copy of the dset layouts
  std::vector<DescriptorSetLayoutInfo> merged_dset_layout_infos = dset_layout_infos;
  // Merge shader bindings with existing program bindings
  // Grow descriptor set layout array if needed, padding gaps with empty layouts
  if (new_dset_layout_infos.size() > merged_dset_layout_infos.size()) {
    merged_dset_layout_infos.resize(new_dset_layout_infos.size());
  }
  // Merge descriptor set layouts. There's significant optimization potential here if
  // this ever becomes a bottleneck; it definitely doesn't scale well as written.
  for (size_t iDS = 0; iDS < new_dset_layout_infos.size(); ++iDS) {
    const DescriptorSetLayoutInfo& src_dset_layout_info = new_dset_layout_infos[iDS];
    DescriptorSetLayoutInfo& dst_dset_layout_info = merged_dset_layout_infos[iDS];
    for (size_t iSB = 0; iSB < src_dset_layout_info.bindings.size(); ++iSB) {
      const auto& src_binding = src_dset_layout_info.bindings[iSB];
      bool found_binding = false;
      for (size_t iDB = 0; iDB < dst_dset_layout_info.bindings.size(); ++iDB) {
        auto& dst_binding = dst_dset_layout_info.bindings[iDB];
        // TODO(https://github.com/cdwfs/spokk/issues/13): need to also compare against arrays starting at lower
        // bindings that intersect this binding.
        if (src_binding.binding == dst_binding.binding) {
          // TODO(https://github.com/cdwfs/spokk/issues/13): validate these asserts with some test cases.
          // For example, some descriptor types may be compatible
          ZOMBO_ASSERT_RETURN(src_binding.descriptorType == dst_binding.descriptorType, -1,
              "set %u binding %u used with different types in two stages", (uint32_t)iDS, src_binding.binding);
          ZOMBO_ASSERT_RETURN(src_binding.descriptorCount == dst_binding.descriptorCount, -2,
              "set %u binding %u used with different array sizes in two stages", (uint32_t)iDS, src_binding.binding);
          // Found a match!
          dst_binding.stageFlags |= src_binding.stageFlags;
          found_binding = true;
          break;
        }
      }
      if (!found_binding) {
        dst_dset_layout_info.bindings.push_back(src_binding);
      }
    }
  }

  // Merge push constant ranges.
  // For now, each shader program only stores a single unified push constant range for all stages.
  // If per-stage ranges ever makes sense, this code will need to change.
  ZOMBO_ASSERT(
      push_constant_ranges.size() == 1, "Currently, all shader stages must share a single push constant range.");
  ZOMBO_ASSERT_RETURN(new_push_constant_ranges.size() <= 1, -3,
      "Currently, all shader stages must share a single push constant range.");
  std::vector<VkPushConstantRange> merged_ranges = push_constant_ranges;
  if (!new_push_constant_ranges.empty()) {
    VkPushConstantRange& merged_range = merged_ranges[0];
    const VkPushConstantRange& new_range = new_push_constant_ranges[0];
    if (merged_range.size == 0) {
      // src is invalid; just copy dst
      merged_range = new_range;
    } else if (new_range.size > 0) {
      // src and dst are both valid; merge them
      uint32_t first_unused_offset = std::max(merged_range.offset + merged_range.size, new_range.offset + new_range.size);
      merged_range.offset = std::min(merged_range.offset, new_range.offset);
      merged_range.size = first_unused_offset - merged_range.offset;
      merged_range.stageFlags |= new_range.stageFlags;
    } else {
      // src is valid; dst is not; do nothing.
    }
  }

  // commit merged changes
  dset_layout_infos.swap(merged_dset_layout_infos);
  push_constant_ranges.swap(merged_ranges);
  return 0;
}

//
// DescriptorPool
//
DescriptorPool::DescriptorPool() {
  ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  ci.flags = 0;  // overriden in finalize()
  ci.poolSizeCount = (uint32_t)pool_sizes.size();
  ci.pPoolSizes = pool_sizes.data();
  for (size_t i = 0; i < VK_DESCRIPTOR_TYPE_RANGE_SIZE; ++i) {
    pool_sizes[i].type = (VkDescriptorType)i;
    pool_sizes[i].descriptorCount = 0;
  }
}
void DescriptorPool::Add(
    uint32_t layout_count, const VkDescriptorSetLayoutCreateInfo* dset_layout_cis, const uint32_t* dsets_per_layout) {
  for (uint32_t iLayout = 0; iLayout < layout_count; ++iLayout) {
    const VkDescriptorSetLayoutCreateInfo& layout = dset_layout_cis[iLayout];
    uint32_t dset_count = (dsets_per_layout != nullptr) ? dsets_per_layout[iLayout] : 1;
    Add(layout, dset_count);
  }
}
void DescriptorPool::Add(const VkDescriptorSetLayoutCreateInfo& dset_layout, uint32_t dset_count) {
  for (uint32_t iBinding = 0; iBinding < dset_layout.bindingCount; ++iBinding) {
    const VkDescriptorSetLayoutBinding& binding = dset_layout.pBindings[iBinding];
    pool_sizes[binding.descriptorType].descriptorCount += binding.descriptorCount * dset_count;
  }
  ci.maxSets += dset_count;
}
VkResult DescriptorPool::Finalize(const Device& device, VkDescriptorPoolCreateFlags flags) {
  ci.flags = flags;
  // descriptor counts can't be zero. Let's just bump them up to one, it won't hurt anything.
  for(auto& pool_size : pool_sizes) {
    if (pool_size.descriptorCount == 0) {
      pool_size.descriptorCount += 1;
    }
  }
  VkResult result = vkCreateDescriptorPool(device, &ci, device.HostAllocator(), &handle);
  return result;
}
void DescriptorPool::Destroy(const Device& device) {
  if (handle != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(device, handle, device.HostAllocator());
    handle = VK_NULL_HANDLE;
  }
}
VkResult DescriptorPool::AllocateSets(const Device& device, uint32_t dset_count,
    const VkDescriptorSetLayout* dset_layouts, VkDescriptorSet* out_dsets) const {
  VkDescriptorSetAllocateInfo alloc_info = {};
  alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info.descriptorPool = handle;
  alloc_info.descriptorSetCount = dset_count;
  alloc_info.pSetLayouts = dset_layouts;
  VkResult result = vkAllocateDescriptorSets(device, &alloc_info, out_dsets);
  return result;
}
VkDescriptorSet DescriptorPool::AllocateSet(const Device& device, VkDescriptorSetLayout dset_layout) const {
  VkDescriptorSet dset = VK_NULL_HANDLE;
  AllocateSets(device, 1, &dset_layout, &dset);
  return dset;
}
void DescriptorPool::FreeSets(Device& device, uint32_t set_count, const VkDescriptorSet* sets) const {
  if (ci.flags & VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT) {
    vkFreeDescriptorSets(device, handle, set_count, sets);
  }
}
void DescriptorPool::FreeSet(Device& device, VkDescriptorSet set) const {
  if (ci.flags & VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT) {
    vkFreeDescriptorSets(device, handle, 1, &set);
  }
}

//
// DescriptorSetWriter
//
DescriptorSetWriter::DescriptorSetWriter(const VkDescriptorSetLayoutCreateInfo& layout_ci)
  : image_infos{}, buffer_infos{}, texel_buffer_views{}, binding_writes{layout_ci.bindingCount} {
  // First time through, we're building a total count of each class of descriptor.
  uint32_t image_count = 0, buffer_count = 0, texel_buffer_count = 0;
  for (uint32_t iBinding = 0; iBinding < layout_ci.bindingCount; ++iBinding) {
    const VkDescriptorSetLayoutBinding& binding = layout_ci.pBindings[iBinding];
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
  for (uint32_t iBinding = 0; iBinding < layout_ci.bindingCount; ++iBinding) {
    const VkDescriptorSetLayoutBinding& binding = layout_ci.pBindings[iBinding];
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
  ZOMBO_ASSERT(next_texel_buffer_view == texel_buffer_views.size(), "invariant failure: texel buffer count mismatch");
  ZOMBO_ASSERT(next_buffer_info == buffer_infos.size(), "invariant failure: buffer count mismatch");
  ZOMBO_ASSERT(next_image_info == image_infos.size(), "invariant failure: image count mismatch");

  // TODO(https://github.com/cdwfs/spokk/issues/16): sort binding_writes by their dstBinding field to allow binary
  // searches later
}
void DescriptorSetWriter::BindCombinedImageSampler(
    VkImageView view, VkImageLayout layout, VkSampler sampler, uint32_t binding, uint32_t array_element) {
  // TODO(https://github.com/cdwfs/spokk/issues/16): replace with a binary search after sorting binding_writes in
  // constructor
  VkWriteDescriptorSet* write = nullptr;
  for (size_t iWrite = 0; iWrite < binding_writes.size(); ++iWrite) {
    if (binding_writes[iWrite].dstBinding == binding) {
      write = &binding_writes[iWrite];
      break;
    }
  }
  ZOMBO_ASSERT(write != nullptr, "binding %u not found in dset", binding);
  ZOMBO_ASSERT(array_element < write->descriptorCount, "array_element %u out of range [0..%u]", array_element,
      write->descriptorCount);
  ZOMBO_ASSERT(write->pImageInfo != nullptr, "binding %u is not an image/sampler descriptor", binding);
  auto* pImageInfo = const_cast<VkDescriptorImageInfo*>(write->pImageInfo);
  pImageInfo->imageView = view;
  pImageInfo->imageLayout = layout;
  pImageInfo->sampler = sampler;
}
void DescriptorSetWriter::BindBuffer(
    VkBuffer buffer, uint32_t binding, VkDeviceSize offset, VkDeviceSize range, uint32_t array_element) {
  // TODO(https://github.com/cdwfs/spokk/issues/16): replace with a binary search after sorting binding_writes in
  // constructor
  VkWriteDescriptorSet* write = nullptr;
  for (size_t iWrite = 0; iWrite < binding_writes.size(); ++iWrite) {
    if (binding_writes[iWrite].dstBinding == binding) {
      write = &binding_writes[iWrite];
      break;
    }
  }
  ZOMBO_ASSERT(write != nullptr, "binding %u not found in dset", binding);
  ZOMBO_ASSERT(array_element < write->descriptorCount, "array_element %u out of range [0..%u]", array_element,
      write->descriptorCount);
  ZOMBO_ASSERT(write->pBufferInfo != nullptr, "binding %u is not a buffer descriptor", binding);
  auto* pBufferInfo = const_cast<VkDescriptorBufferInfo*>(write->pBufferInfo);
  pBufferInfo->buffer = buffer;
  pBufferInfo->offset = offset;
  pBufferInfo->range = range;
}
void DescriptorSetWriter::BindTexelBuffer(VkBufferView view, uint32_t binding, uint32_t array_element) {
  // TODO(https://github.com/cdwfs/spokk/issues/16): replace with a binary search after sorting binding_writes in
  // constructor
  VkWriteDescriptorSet* write = nullptr;
  for (size_t iWrite = 0; iWrite < binding_writes.size(); ++iWrite) {
    if (binding_writes[iWrite].dstBinding == binding) {
      write = &binding_writes[iWrite];
      break;
    }
  }
  ZOMBO_ASSERT(write != nullptr, "binding %u not found in dset", binding);
  ZOMBO_ASSERT(array_element < write->descriptorCount, "array_element %u out of range [0..%u]", array_element,
      write->descriptorCount);
  ZOMBO_ASSERT(write->pTexelBufferView != nullptr, "binding %u is not a texel buffer descriptor", binding);
  auto* pTexelBufferView = const_cast<VkBufferView*>(write->pTexelBufferView);
  *pTexelBufferView = view;
}
void DescriptorSetWriter::WriteAll(const Device& device, VkDescriptorSet dest_set) {
  for (auto& write : binding_writes) {
    write.dstSet = dest_set;
  }
  vkUpdateDescriptorSets(device, (uint32_t)binding_writes.size(), binding_writes.data(), 0, nullptr);
}
void DescriptorSetWriter::WriteOne(
    const Device& device, VkDescriptorSet dest_set, uint32_t binding, uint32_t array_element) {
  ZOMBO_ASSERT(
      binding < binding_writes.size(), "binding %u out of range [0..%u]", binding, (uint32_t)binding_writes.size());
  VkWriteDescriptorSet writeCopy = binding_writes[binding];
  writeCopy.dstSet = dest_set;
  writeCopy.dstArrayElement = array_element;
  if (writeCopy.pImageInfo != nullptr) {
    writeCopy.pImageInfo += array_element;
  } else if (writeCopy.pBufferInfo != nullptr) {
    writeCopy.pBufferInfo += array_element;
  } else if (writeCopy.pTexelBufferView != nullptr) {
    writeCopy.pTexelBufferView += array_element;
  }
  vkUpdateDescriptorSets(device, 1, &writeCopy, 0, nullptr);
}

}  // namespace spokk
