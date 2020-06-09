#pragma once

#include "spokk_device.h"

#include <array>
#include <map>
#include <string>
#include <vector>

struct SpvReflectShaderModule;  // from spirv_reflect.h
struct SpvReflectDescriptorBinding;  // from spirv_reflect.h

namespace spokk {

struct ShaderInputAttribute {
  std::string name;
  uint32_t location;
  VkFormat format;
};
struct DescriptorBindPoint {
  uint32_t set;
  uint32_t binding;
};
struct DescriptorSetLayoutInfo {
  std::vector<VkDescriptorSetLayoutBinding> bindings;
};

struct Shader {
  Shader() {}

  VkResult CreateAndLoadSpirvFile(const Device& device, const std::string& filename);
  VkResult CreateAndLoadSpirvFp(const Device& device, FILE* fp, int len_bytes);
  VkResult CreateAndLoadSpirvMem(const Device& device, const void* buffer, int len_bytes);

  // After parsing, you can probably get rid of the SPIRV to save some memory.
  void UnloadSpirv(void) { spirv = std::vector<uint32_t>(0); }
  // Dynamic buffers need a different descriptor type, but there's no way to express it in the shader language.
  // So for now, you have to force individual buffers to be dynamic.
  void OverrideDescriptorType(uint32_t dset, uint32_t binding, VkDescriptorType new_type);

  // Look up the bind point for a descriptor, by name. This is not fast; if you need the results more than once,
  // avoid multiple calls and cache the return value yourself.
  DescriptorBindPoint GetDescriptorBindPoint(const std::string& name) const;

  void Destroy(const Device& device);

  VkShaderModule handle = VK_NULL_HANDLE;
  std::vector<uint32_t> spirv = {};  // May be empty if UnloadSpirv() has been called after a successful load
  VkShaderStageFlagBits stage = (VkShaderStageFlagBits)0;
  std::string entry_point = "main";
  // Resources used by this shader:
  std::vector<ShaderInputAttribute> input_attributes = {};
  std::vector<DescriptorSetLayoutInfo> dset_layout_infos = {};  // one per dset (including empty ones)
  VkPushConstantRange push_constant_range = {};  // range.size = 0 means this stage doesn't use push constants.
private:
  VkResult ParseSpirvAndCreate(const Device& device);
  void ParseShaderResources(const SpvReflectShaderModule& refl_module);
  void AddShaderResourceToDescriptorSetLayout(const SpvReflectDescriptorBinding& new_binding);

  std::map<std::string, DescriptorBindPoint> name_to_index_ = {};  // one per binding across all dsets in this Shader.
};

struct ShaderProgram {
  ShaderProgram() {}

  ShaderProgram(const ShaderProgram& rhs) = delete;
  ShaderProgram& operator=(const ShaderProgram& rhs) = delete;

  VkResult AddShader(const Shader* shader);
  static VkResult ForceCompatibleLayoutsAndFinalize(const Device& device, const std::vector<ShaderProgram*> programs);
  VkResult Finalize(const Device& device);
  void Destroy(const Device& device);

  const std::vector<ShaderInputAttribute>* input_attributes =
      nullptr;  // points to the vector in the appropriate Shader
  std::vector<VkDescriptorSetLayoutCreateInfo> dset_layout_cis =
      {};  // one per dset. Unused sets are padded with empty layouts.
  std::vector<DescriptorSetLayoutInfo> dset_layout_infos =
      {};  // one per dset. Unused sets are padded with empty layouts.
  std::vector<VkPushConstantRange> push_constant_ranges = {
      {(VkShaderStageFlagBits)0, 0, 0}};  // one per active stage that uses push constants.

  std::vector<VkPipelineShaderStageCreateInfo> shader_stage_cis =
      {};  // one per active stage. used to create graphics pipelines
  std::vector<std::string> entry_point_names = {};  // one per active stage.

  VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
  std::vector<VkDescriptorSetLayout> dset_layouts = {};  // one per dset (including empty ones)

  VkShaderStageFlags active_stages = 0;

private:
  // Attempts to incorporate the provided dset layouts and push constant ranges into this shader program.
  // returns non-zero if an incompatibility was detected, in which case no changes are made.
  int MergeLayouts(const std::vector<DescriptorSetLayoutInfo>& new_dset_layout_infos,
      const std::vector<VkPushConstantRange> new_push_constant_ranges);
};

struct DescriptorPool {
  DescriptorPool();

  // Adds a number of instances of each type of dset in the array. This would be pretty easy to call on a ShaderProgram.
  // if dsets_per_layout is nullptr, assume one of each layout.
  // TODO(cort): add() really needs a better name.
  void Add(uint32_t layout_count, const VkDescriptorSetLayoutCreateInfo* dset_layout_cis,
      const uint32_t* dsets_per_layout = nullptr);
  // Shortcut to add a single dset layout
  void Add(const VkDescriptorSetLayoutCreateInfo& dset_layout, uint32_t dset_count = 1);

  VkResult Finalize(const Device& device, VkDescriptorPoolCreateFlags flags = 0);
  void Destroy(const Device& device);

  // implicit conversion to VkDescriptorPool
  operator VkDescriptorPool() const { return handle; }

  VkResult AllocateSets(const Device& device, uint32_t dset_count, const VkDescriptorSetLayout* dset_layouts,
      VkDescriptorSet* out_dsets) const;
  VkDescriptorSet AllocateSet(const Device& device, VkDescriptorSetLayout dset_layout) const;
  // Only if VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT is set at creation time
  void FreeSets(Device& device, uint32_t set_count, const VkDescriptorSet* sets) const;
  void FreeSet(Device& device, VkDescriptorSet set) const;

  VkDescriptorPool handle = VK_NULL_HANDLE;
  VkDescriptorPoolCreateInfo ci = {};
  std::vector<VkDescriptorPoolSize> pool_sizes = {};
};

struct DescriptorSetWriter {
  explicit DescriptorSetWriter(const VkDescriptorSetLayoutCreateInfo& layout_ci);

  void BindImage(VkImageView view, VkImageLayout layout, uint32_t binding, uint32_t array_element = 0) {
    return BindCombinedImageSampler(view, layout, VK_NULL_HANDLE, binding, array_element);
  }
  void BindSampler(VkSampler sampler, uint32_t binding, uint32_t array_element = 0) {
    return BindCombinedImageSampler(VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED, sampler, binding, array_element);
  }
  void BindCombinedImageSampler(
      VkImageView view, VkImageLayout layout, VkSampler sampler, uint32_t binding, uint32_t array_element = 0);
  void BindBuffer(VkBuffer buffer, uint32_t binding, VkDeviceSize offset = 0, VkDeviceSize range = VK_WHOLE_SIZE,
      uint32_t array_element = 0);
  void BindTexelBuffer(VkBufferView view, uint32_t binding, uint32_t array_element = 0);

  void WriteAll(const Device& device, VkDescriptorSet dest_set);
  void WriteOne(const Device& device, VkDescriptorSet dest_set, uint32_t binding, uint32_t array_element = 0);

  // Walk through the layout and build the following lists:
  std::vector<VkDescriptorImageInfo> image_infos;
  std::vector<VkDescriptorBufferInfo> buffer_infos;
  std::vector<VkBufferView> texel_buffer_views;
  std::vector<VkWriteDescriptorSet> binding_writes;  // one per binding. Sparse dsets are valid, but discouraged.
};

}  // namespace spokk
