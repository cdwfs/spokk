#if !defined(VK_SHADER_H)
#define VK_SHADER_H

#if defined(SPOKK_ENABLE_SHADERC)
# include <shaderc/shaderc.hpp>
#endif  // defined(SPOKK_ENABLE_SHADERC)

#include <array>
#include <vector>

namespace spokk {

struct DescriptorSetLayoutBindingInfo {
  // The name of each binding in a given shader stage. Purely for debugging.
  std::vector< std::tuple<VkShaderStageFlagBits, std::string> > stage_names;
};
struct DescriptorSetLayoutInfo {
  std::vector<VkDescriptorSetLayoutBinding> bindings;
  std::vector<DescriptorSetLayoutBindingInfo> binding_infos;  // one per binding
};

#if defined(SPOKK_ENABLE_SHADERC)
class ShaderCompiler {
public:
  ShaderCompiler();
  ~ShaderCompiler();

  shaderc::SpvCompilationResult compile_glsl_string(const char *glsl_source,
    const std::string& logging_name, const std::string& entry_point = "main",
    VkShaderStageFlagBits target_stage = VK_SHADER_STAGE_ALL,
    const shaderc::CompileOptions& options = shaderc::CompileOptions()) const;
  shaderc::SpvCompilationResult compile_glsl_fp(FILE *fp, int len_bytes,
    const std::string& logging_name, const std::string& entry_point = "main",
    VkShaderStageFlagBits target_stage = VK_SHADER_STAGE_ALL,
    const shaderc::CompileOptions& options = shaderc::CompileOptions()) const;
  shaderc::SpvCompilationResult compile_glsl_file(const std::string& filename,
    const std::string& entry_point = "main",
    VkShaderStageFlagBits target_stage = VK_SHADER_STAGE_ALL,
    const shaderc::CompileOptions& = shaderc::CompileOptions()) const;

protected:
  shaderc::Compiler compiler_;
};
#endif

struct Shader {
  Shader() : handle(VK_NULL_HANDLE), spirv{}, stage((VkShaderStageFlagBits)0), dset_layout_infos{}, push_constant_range{} {}
  VkResult create_and_load_spv_file(const DeviceContext& device_context, const std::string& filename);
  VkResult create_and_load_spv_fp(const DeviceContext& device_context, FILE *fp, int len_bytes);
  VkResult create_and_load_spv_mem(const DeviceContext& device_context, const void *buffer, int len_bytes);
#if defined(SPOKK_ENABLE_SHADERC)
  VkResult create_and_load_compile_result(const DeviceContext& device_context, const shaderc::SpvCompilationResult& result) {
    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
      return VK_ERROR_INITIALIZATION_FAILED;
    }
    return create_and_load_spv_mem(device_context, result.cbegin(),
      static_cast<int>( (result.cend() - result.cbegin()) * sizeof(uint32_t) ));
  }
#endif  // defined(SPOKK_ENABLE_SHADERC)
  // After parsing, you can probably get rid of the SPIRV to save some memory.
  void unload_spirv(void) {
    spirv = std::vector<uint32_t>(0);
  }
  // Dynamic buffers need a different descriptor type, but there's no way to express it in the shader language.
  // So for now, you have to force individual buffers to be dynamic.
  // TODO(cort): would it be better to do this at the ShaderPipeline level?
  void override_descriptor_type(uint32_t dset, uint32_t binding, VkDescriptorType new_type);
  void destroy(const DeviceContext& device_context);

  VkShaderModule handle;
  std::vector<uint32_t> spirv;
  VkShaderStageFlagBits stage;
  // Resources used by this shader:
  std::vector<DescriptorSetLayoutInfo> dset_layout_infos;
  VkPushConstantRange push_constant_range;  // range.size = 0 means this stage doesn't use push constants.
private:
  VkResult parse_spirv_and_create(const DeviceContext& device_context);
};

struct ShaderPipeline {
  ShaderPipeline() : dset_layout_cis{}, dset_layout_infos{}, push_constant_ranges{}, shader_stage_cis{},
    entry_point_names{}, pipeline_layout(VK_NULL_HANDLE), dset_layouts{}, active_stages(0) {
  }
  VkResult add_shader(const Shader *shader, const char *entry_point = "main");
  static VkResult force_compatible_layouts_and_finalize(const DeviceContext& device_context,
    const std::vector<ShaderPipeline*> pipelines);
  VkResult finalize(const DeviceContext& device_context);
  void destroy(const DeviceContext& device_context);

  std::vector<VkDescriptorSetLayoutCreateInfo> dset_layout_cis; // one per dset
  std::vector<DescriptorSetLayoutInfo> dset_layout_infos; // one per dset
  std::vector<VkPushConstantRange> push_constant_ranges;  // one per active stage that uses push constants.

  std::vector<VkPipelineShaderStageCreateInfo> shader_stage_cis;  // one per active stage. used to create graphics pipelines
  std::vector<std::string> entry_point_names;  // one per active stage.

  VkPipelineLayout pipeline_layout;
  std::vector<VkDescriptorSetLayout> dset_layouts;  // one per dset

  VkShaderStageFlags active_stages;
};

struct DescriptorPool {
  DescriptorPool();

  // Adds a number of instances of each type of dset in the array. This would be pretty easy to call on a ShaderPipeline.
  // if dsets_per_layout is nullptr, assume one of each layout.
  void add(uint32_t layout_count, const VkDescriptorSetLayoutCreateInfo* dset_layout_cis, const uint32_t* dsets_per_layout = nullptr);
  // Shortcut to add a single dset layout
  void add(const VkDescriptorSetLayoutCreateInfo& dset_layout, uint32_t dset_count = 1);

  VkResult finalize(const DeviceContext& device_context, VkDescriptorPoolCreateFlags flags = 0);
  void destroy(const DeviceContext& device_context);

  VkResult allocate_sets(const DeviceContext& device_context, uint32_t dset_count, const VkDescriptorSetLayout *dset_layouts, VkDescriptorSet *out_dsets) const;
  VkDescriptorSet allocate_set(const DeviceContext& device_context, VkDescriptorSetLayout dset_layout) const;
  // Only if VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT is set at creation time
  void free_sets(DeviceContext& device_context, uint32_t set_count, const VkDescriptorSet* sets) const;
  void free_set(DeviceContext& device_context, VkDescriptorSet set) const;

  VkDescriptorPool handle;
  VkDescriptorPoolCreateInfo ci;
  std::array<VkDescriptorPoolSize, VK_DESCRIPTOR_TYPE_RANGE_SIZE> pool_sizes;
};

struct DescriptorSetWriter {
  explicit DescriptorSetWriter(const VkDescriptorSetLayoutCreateInfo &layout_ci);

  void bind_image(VkImageView view, VkImageLayout layout, VkSampler sampler, uint32_t binding, uint32_t array_element = 0);
  void bind_buffer(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range, uint32_t binding, uint32_t array_element = 0);
  void bind_texel_buffer(VkBufferView view, uint32_t binding, uint32_t array_element = 0);

  void write_all_to_dset(const DeviceContext& device_context, VkDescriptorSet dset);
  void write_one_to_dset(const DeviceContext& device_context, VkDescriptorSet dset, uint32_t binding, uint32_t array_element = 0);

  // Walk through the layout and build the following lists:
  std::vector<VkDescriptorImageInfo> image_infos;
  std::vector<VkDescriptorBufferInfo> buffer_infos;
  std::vector<VkBufferView> texel_buffer_views;
  std::vector<VkWriteDescriptorSet> binding_writes; // one per binding. Sparse dsets are valid, but discouraged.
};

}  // namespace spokk

#endif  // !defined(VK_SHADER_H)