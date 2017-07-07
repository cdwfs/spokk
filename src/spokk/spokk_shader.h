#pragma once

#include "spokk_context.h"

#include <array>
#include <vector>

namespace spokk {

struct DescriptorSetLayoutBindingInfo {
  // The name of each binding in a given shader stage. Purely for debugging.
  std::vector<std::tuple<VkShaderStageFlagBits, std::string> > stage_names;
};
struct DescriptorSetLayoutInfo {
  std::vector<VkDescriptorSetLayoutBinding> bindings;
  std::vector<DescriptorSetLayoutBindingInfo> binding_infos;  // one per binding
};
struct DescriptorBindPoint {
  uint32_t set;
  uint32_t binding;
};

struct Shader {
  Shader()
    : handle(VK_NULL_HANDLE), spirv{}, stage((VkShaderStageFlagBits)0), dset_layout_infos{}, push_constant_range{} {}

  VkResult CreateAndLoadSpirvFile(const DeviceContext& device_context, const std::string& filename);
  VkResult CreateAndLoadSpirvFp(const DeviceContext& device_context, FILE* fp, int len_bytes);
  VkResult CreateAndLoadSpirvMem(const DeviceContext& device_context, const void* buffer, int len_bytes);

  // After parsing, you can probably get rid of the SPIRV to save some memory.
  void UnloadSpirv(void) { spirv = std::vector<uint32_t>(0); }
  // Dynamic buffers need a different descriptor type, but there's no way to express it in the shader language.
  // So for now, you have to force individual buffers to be dynamic.
  void OverrideDescriptorType(uint32_t dset, uint32_t binding, VkDescriptorType new_type);

  // Look up the bind point for a descriptor, by name. This is not fast; if you need the results more than once,
  // avoid multiple calls and cache the return value yourself.
  // The stage_mask parameter can optionally be used to limit the search to particular shader stages, to disambiguate
  // in cases where the same name is used for different bind points in different stages within a single ShaderProgram.
  DescriptorBindPoint GetDescriptorBindPoint(const std::string& name) const;

  void Destroy(const DeviceContext& device_context);

  VkShaderModule handle;
  std::vector<uint32_t> spirv;
  VkShaderStageFlagBits stage;
  // Resources used by this shader:
  std::vector<DescriptorSetLayoutInfo> dset_layout_infos;
  VkPushConstantRange push_constant_range;  // range.size = 0 means this stage doesn't use push constants.
private:
  VkResult ParseSpirvAndCreate(const DeviceContext& device_context);
};

struct ShaderProgram {
  ShaderProgram()
    : dset_layout_cis{},
      dset_layout_infos{},
      push_constant_ranges{},
      shader_stage_cis{},
      entry_point_names{},
      pipeline_layout(VK_NULL_HANDLE),
      dset_layouts{},
      active_stages(0) {}

  ShaderProgram(const ShaderProgram& rhs) = delete;
  ShaderProgram& operator=(const ShaderProgram& rhs) = delete;

  VkResult AddShader(const Shader* shader, const char* entry_point = "main");
  static VkResult ForceCompatibleLayoutsAndFinalize(
      const DeviceContext& device_context, const std::vector<ShaderProgram*> programs);
  VkResult Finalize(const DeviceContext& device_context);
  void Destroy(const DeviceContext& device_context);

  std::vector<VkDescriptorSetLayoutCreateInfo> dset_layout_cis;  // one per dset
  std::vector<DescriptorSetLayoutInfo> dset_layout_infos;  // one per dset
  std::vector<VkPushConstantRange> push_constant_ranges;  // one per active stage that uses push constants.

  std::vector<VkPipelineShaderStageCreateInfo>
      shader_stage_cis;  // one per active stage. used to create graphics pipelines
  std::vector<std::string> entry_point_names;  // one per active stage.

  VkPipelineLayout pipeline_layout;
  std::vector<VkDescriptorSetLayout> dset_layouts;  // one per dset

  VkShaderStageFlags active_stages;
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

  VkResult Finalize(const DeviceContext& device_context, VkDescriptorPoolCreateFlags flags = 0);
  void Destroy(const DeviceContext& device_context);

  VkResult AllocateSets(const DeviceContext& device_context, uint32_t dset_count,
      const VkDescriptorSetLayout* dset_layouts, VkDescriptorSet* out_dsets) const;
  VkDescriptorSet AllocateSet(const DeviceContext& device_context, VkDescriptorSetLayout dset_layout) const;
  // Only if VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT is set at creation time
  void FreeSets(DeviceContext& device_context, uint32_t set_count, const VkDescriptorSet* sets) const;
  void FreeSet(DeviceContext& device_context, VkDescriptorSet set) const;

  VkDescriptorPool handle;
  VkDescriptorPoolCreateInfo ci;
  std::array<VkDescriptorPoolSize, VK_DESCRIPTOR_TYPE_RANGE_SIZE> pool_sizes;
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

  void WriteAll(const DeviceContext& device_context, VkDescriptorSet dest_set);
  void WriteOne(
      const DeviceContext& device_context, VkDescriptorSet dest_set, uint32_t binding, uint32_t array_element = 0);

  // Walk through the layout and build the following lists:
  std::vector<VkDescriptorImageInfo> image_infos;
  std::vector<VkDescriptorBufferInfo> buffer_infos;
  std::vector<VkBufferView> texel_buffer_views;
  std::vector<VkWriteDescriptorSet> binding_writes;  // one per binding. Sparse dsets are valid, but discouraged.
};

}  // namespace spokk
