/*

NOTE: most cases of frame & pframe below should really be view / pview.


Globals:
- set 0, binding 0:
  - camera params (view, proj, viewproj, inverses)
    - NOTE: we will likely be rendering multiple views per frame. Maybe should be separate from time?
  - eye pos, eye dir
    - can extract this from view mat, right?
  - wall_time
  - game_time
  - viewport
- set 0, binding 1:
  - lights
  
Materials:
- set 1, binding 0:
  - reserved
  
MeshInstance:
- if using dynamic uniform buffers, there's a minimum alignment restriction of 256 bytes.
  - world -- unique per frame, right?
  - worldview -- changes per view
  - worldviewproj  -- changes per view
  - 64 bytes of filler
  */

#include "spokk_mesh.h"
#include "spokk_pipeline.h"
#include "spokk_shader_interface.h"

#include <mathfu/glsl_mappings.h>

namespace spokk {

struct Transform {
  mathfu::vec3 pos;
  float scale;
  mathfu::quat orientation;
};

class Material {
  const GraphicsPipeline* pipeline;
  std::vector<VkDescriptorSet> material_dsets;  // pipelined
};

class MeshInstance {
public:
  const Mesh* mesh_;
  VkDeviceSize const_buffer_offset_;  // offset into renderer's instance_const_buffers_ buffer
  std::vector<VkDescriptorSet> instance_dsets;  // pipelined
  bool is_active_;
  Transform transform_;
};

class Renderer {
public:
  static const int MAX_INSTANCE_COUNT = 1000;

  MeshInstance* CreateInstance(const Mesh* mesh, const Material* material);
  // ignore DeleteInstance for now

  void RenderView(const mathfu::mat4& view, const mathfu::mat4& proj);

private:
  PipelinedBuffer world_const_buffers_;  // sizeof(WorldConstants)
  PipelinedBuffer instance_const_buffers_;  // MAX_INSTANCE_COUNT * sizeof(InstanceConstants)

  std::vector<VkDescriptorSet> global_dsets;  // one per pframe

  std::vector<MeshInstance> instances_;
  uint32_t pframe_index_, pframe_count_;
};

}  // namespace spokk

namespace spokk {

MeshInstance* Renderer::CreateInstance(const Mesh* mesh, const Material* material) {
  (void)mesh;
  (void)material;
  return nullptr;
}

void Renderer::RenderView(const mathfu::mat4& view, const mathfu::mat4& proj) {
  // advance pframe
  pframe_index_ = (pframe_index_ + 1) % pframe_count_;

  // Fill in the instance constant buffer.
  // bake each mesh instance's transform to a world matrix,
  // and then precompute variants of this matrix for use by the VS
  InstanceTransforms* instance_xforms = (InstanceTransforms*)instance_const_buffers_.Mapped(pframe_index_);
  for (size_t i = 0; i < instances_.size(); ++i) {
    if (instances_[i].is_active_) {
      const Transform& t = instances_[i].transform_;
      mathfu::mat4 world = mathfu::mat4::FromTranslationVector(t.pos) * t.orientation.ToMatrix4() *
          mathfu::mat4::FromScaleVector(mathfu::vec3(t.scale, t.scale, t.scale));
      mathfu::mat4 world_view = view * world;
      mathfu::mat4 world_view_proj = proj * world_view;
      mathfu::mat4 world_inv = world.Inverse();
      // TODO(cort): ensure the compiler doesn't try to read from write-combining memory here
      instance_xforms[i].world = world;
      instance_xforms[i].world_view = world_view;
      instance_xforms[i].world_view_proj = world_view_proj;
      instance_xforms[i].world_inv = world_inv;
    }
  }

  /*
  precondition: world transforms are baked
  1. visibility culling.
     - output is a bool per instance if active & visible
  2. render (naive)
     - bind world/camera const dset
     - for each active/visible instance:
       - bind material pipeline
       - bind material dset
       - bind mesh VB/IB
       - bind instance dset with appropriate dynamic offset
       - vkCmdDraw
  2. render (clever)
     - bind world/camera const dset
     - for each material bucket:
       - bind material pipeline
       - bind material dset
       - for each mesh bucket:
         - bind mesh VB/IB
         - for each active/visible instance of this mesh:
           - bind instance dset with appropriate dynamic offset
           - vkCmdDraw
  TODO:
  - how to leverage instanced draws?
    - Need to ensure all instances of a given mesh have contiguous constant buffers
  - how to avoid requiring MAX_NUM_VIEWS full copies of every resource in memory simultaneously?
    - ring buffers ahoy.
    - but it's not THAT awful, is it? 10000 instances is 2.5MB. How many views in a scene? 32?
      that's 80 MB. Still not really breaking a sweat.
  - How do render passes fit in?
    - can there be multiple views per render pass?
      - shadow map: gosh, I don't know. Naively, one view for the pass, but that may not be true.
      - cubemap gen: ugh. six views per pass? one pass, rendered with six different views to six different FBs?
      - deferred: one view, multiple subpasses. But this seems like a different RenderView function.
  */
}

}  // namespace spokk