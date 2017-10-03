#include "spokk_device.h"
#include "spokk_mesh.h"
#include "spokk_pipeline.h"

#include <mathfu/glsl_mappings.h>

namespace spokk {

struct Transform {
  mathfu::vec3 pos;
  float scale;
  mathfu::quat orientation;
};

class Material {
public:
  const GraphicsPipeline* pipeline;
  std::vector<VkDescriptorSet> material_dsets;  // pipelined
};

class MeshInstance {
public:
  const Mesh* mesh_;
  const Material* material_;
  std::vector<VkDescriptorSet> instance_dsets;  // pipelined
  bool is_active_;
  Transform transform_;
};

class Renderer {
public:
  Renderer();
  ~Renderer();

  struct CreateInfo {
    uint32_t pframe_count;
  };

  int Create(const Device& device, const CreateInfo& ci);
  void Destroy(const Device& device);

  MeshInstance* CreateInstance(const Mesh* mesh, const Material* material);
  // ignore DeleteInstance for now

  void RenderView(
      VkCommandBuffer cb, const mathfu::mat4& view, const mathfu::mat4& proj, const mathfu::vec4& time_and_res);

  const std::vector<DescriptorSetLayoutInfo> GetCommonDescriptorSetLayoutInfos(void) const {
    DescriptorSetLayoutInfo empty_material_layout = {};
    return {global_dset_layout_info_, empty_material_layout, instance_dset_layout_info_};
  }

private:
  Renderer& operator=(const Renderer& rhs) = delete;
  Renderer& operator=(const Renderer&& rhs) = delete;
  Renderer(const Renderer& rhs) = delete;
  Renderer(const Renderer&& rhs) = delete;

  PipelinedBuffer world_const_buffers_;  // sizeof(WorldConstants)
  PipelinedBuffer instance_const_buffers_;  // MAX_INSTANCE_COUNT * sizeof(InstanceConstants)

  DescriptorSetLayoutInfo global_dset_layout_info_;
  DescriptorSetLayoutInfo instance_dset_layout_info_;

  std::vector<VkDescriptorSet> global_dsets_;  // one per pframe
  std::vector<VkDescriptorSet> instance_dsets_;  // MAX_INSTANCE_COUNT * pframe_count
  static const int MAX_INSTANCE_COUNT = 1024;
  std::vector<MeshInstance> instances_;
  uint32_t pframe_index_ = 0, pframe_count_ = 0;

  // TODO(cort): this makes me deeply uncomfortable, but can't think of an alternative right now.
  VkDescriptorSetLayout world_dset_layout_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout instance_dset_layout_ = VK_NULL_HANDLE;
  DescriptorPool dpool_ = {};
};

}  // namespace spokk
