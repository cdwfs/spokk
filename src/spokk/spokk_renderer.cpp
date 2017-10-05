#include "spokk_renderer.h"

#include "spokk_debug.h"
#include "spokk_platform.h"
#include "spokk_shader_interface.h"

namespace {
#if defined(ZOMBO_COMPILER_MSVC)
#pragma float_control(precise, on, push)
#endif
glm::vec3 ExtractViewPos(const glm::mat4& view) {
  glm::mat3 view_rot(view[0][0], view[0][1], view[0][2], view[1][0], view[1][1], view[1][2], view[2][0], view[2][1], view[2][2]);
  glm::vec3 d(view[3][0], view[3][1], view[3][2]);
  return -d * view_rot;
}
#if defined(ZOMBO_COMPILER_MSVC)
#pragma float_control(pop)
#endif
}  // namespace

namespace spokk {

Renderer::Renderer() { instances_.reserve(MAX_INSTANCE_COUNT); }
Renderer::~Renderer() {}

int Renderer::Create(const Device& device, const CreateInfo& ci) {
  pframe_count_ = ci.pframe_count;

  // technically, we can work with this, but let's be strict & not waste memory
  ZOMBO_ASSERT_RETURN((sizeof(InstanceTransforms) % device.Properties().limits.minUniformBufferOffsetAlignment) == 0,
      -1, "sizeof(InstanceTranforms) [%d] is not disibile by device's minUniformBufferOffsetAlignment [%d]",
      (int)sizeof(InstanceTransforms), (int)device.Properties().limits.minUniformBufferOffsetAlignment);

  VkBufferCreateInfo world_const_buffer_ci = {};
  world_const_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  world_const_buffer_ci.size = sizeof(spokk::CameraConstants);
  world_const_buffer_ci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  world_const_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  SPOKK_VK_CHECK(
      world_const_buffers_.Create(device, ci.pframe_count, world_const_buffer_ci, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));

  VkBufferCreateInfo instance_const_buffer_ci = {};
  instance_const_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  instance_const_buffer_ci.size = MAX_INSTANCE_COUNT * sizeof(InstanceTransforms);
  instance_const_buffer_ci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  instance_const_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  SPOKK_VK_CHECK(instance_const_buffers_.Create(
      device, ci.pframe_count, instance_const_buffer_ci, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));

  // define descriptor set layouts. TODO(cort): pull these from a representative shader instead of hard-coding.
  global_dset_layout_info_.bindings = {
      // clang-format off
      {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL_GRAPHICS, nullptr},
      // clang-format on
  };
  VkDescriptorSetLayoutCreateInfo world_dset_layout_ci = {};
  world_dset_layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  world_dset_layout_ci.bindingCount = (uint32_t)global_dset_layout_info_.bindings.size();
  world_dset_layout_ci.pBindings = global_dset_layout_info_.bindings.data();
  SPOKK_VK_CHECK(
      vkCreateDescriptorSetLayout(device, &world_dset_layout_ci, device.HostAllocator(), &world_dset_layout_));

  instance_dset_layout_info_.bindings = {
      // clang-format off
      {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL_GRAPHICS, nullptr},
      // clang-format on
  };
  VkDescriptorSetLayoutCreateInfo instance_dset_layout_ci = {};
  instance_dset_layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  instance_dset_layout_ci.bindingCount = (uint32_t)instance_dset_layout_info_.bindings.size();
  instance_dset_layout_ci.pBindings = instance_dset_layout_info_.bindings.data();
  SPOKK_VK_CHECK(
      vkCreateDescriptorSetLayout(device, &instance_dset_layout_ci, device.HostAllocator(), &instance_dset_layout_));

  dpool_.Add(world_dset_layout_ci, ci.pframe_count);
  dpool_.Add(instance_dset_layout_ci, MAX_INSTANCE_COUNT * ci.pframe_count);
  SPOKK_VK_CHECK(dpool_.Finalize(device));

  // allocate and populate global dsets
  global_dsets_.resize(ci.pframe_count);
  std::vector<VkDescriptorSetLayout> global_dset_alloc_layouts(global_dsets_.size(), world_dset_layout_);
  SPOKK_VK_CHECK(dpool_.AllocateSets(
      device, (uint32_t)global_dset_alloc_layouts.size(), global_dset_alloc_layouts.data(), global_dsets_.data()));
  DescriptorSetWriter global_dset_writer(world_dset_layout_ci);
  for (uint32_t i = 0; i < global_dsets_.size(); ++i) {
    global_dset_writer.BindBuffer(world_const_buffers_.Handle(i), 0);
    global_dset_writer.WriteAll(device, global_dsets_[i]);
  }

  // allocate and populate instance dsets
  instance_dsets_.resize(MAX_INSTANCE_COUNT * ci.pframe_count);
  std::vector<VkDescriptorSetLayout> instance_dset_alloc_layouts(instance_dsets_.size(), instance_dset_layout_);
  SPOKK_VK_CHECK(dpool_.AllocateSets(device, (uint32_t)instance_dset_alloc_layouts.size(),
      instance_dset_alloc_layouts.data(), instance_dsets_.data()));
  DescriptorSetWriter instance_dset_writer(instance_dset_layout_ci);
  for (uint32_t i = 0; i < instance_dsets_.size(); ++i) {
    uint32_t pframe = i % ci.pframe_count;
    uint32_t instance = i / ci.pframe_count;
    instance_dset_writer.BindBuffer(
        instance_const_buffers_.Handle(pframe), 0, instance * sizeof(InstanceTransforms), sizeof(InstanceTransforms));
    instance_dset_writer.WriteAll(device, instance_dsets_[i]);
  }

  return 0;
}
void Renderer::Destroy(const Device& device) {
  vkDestroyDescriptorSetLayout(device, world_dset_layout_, device.HostAllocator());
  vkDestroyDescriptorSetLayout(device, instance_dset_layout_, device.HostAllocator());
  vkDestroyDescriptorPool(device, dpool_, device.HostAllocator());
  instance_const_buffers_.Destroy(device);
  world_const_buffers_.Destroy(device);
}

MeshInstance* Renderer::CreateInstance(const Mesh* mesh, const Material* material) {
  if (instances_.size() >= MAX_INSTANCE_COUNT) {
    return nullptr;
  }
  uint32_t index = (uint32_t)instances_.size();
  instances_.push_back({});
  instances_[index].mesh_ = mesh;
  instances_[index].material_ = material;
  instances_[index].instance_dsets.clear();
  instances_[index].instance_dsets.insert(instances_[index].instance_dsets.end(),
      instance_dsets_.begin() + pframe_count_ * index, instance_dsets_.begin() + pframe_count_ * (index + 1));
  instances_[index].is_active_ = true;
  instances_[index].transform_.pos = glm::vec3(0, 0, 0);
  instances_[index].transform_.orientation = glm::quat_identity<float,glm::highp>();
  instances_[index].transform_.scale = 1.0f;
  return &instances_[index];
}

void Renderer::RenderView(
    VkCommandBuffer cb, const glm::mat4& view, const glm::mat4& proj, const glm::vec4& time_and_res) {
  // advance pframe
  pframe_index_ = (pframe_index_ + 1) % pframe_count_;

  // Fill in the instance constant buffer.
  // bake each mesh instance's transform to a world matrix,
  // and then precompute variants of this matrix for use by the VS
  InstanceTransforms* instance_xforms = (InstanceTransforms*)instance_const_buffers_.Mapped(pframe_index_);
  for (size_t i = 0; i < instances_.size(); ++i) {
    if (instances_[i].is_active_) {
      const Transform& t = instances_[i].transform_;
      glm::mat4 world = ComposeTransform(t.pos, t.orientation, t.scale);
      glm::mat4 world_view = view * world;
      glm::mat4 world_view_proj = proj * world_view;
      glm::mat4 world_inv = glm::inverse(world);
      // TODO(cort): ensure the compiler doesn't try to read from write-combining memory here
      instance_xforms[i].world = world;
      instance_xforms[i].world_view = world_view;
      instance_xforms[i].world_view_proj = world_view_proj;
      instance_xforms[i].world_inv = world_inv;
    }
  }
  instance_const_buffers_.FlushPframeHostCache(pframe_index_);

  // Fill in world constant buffer
  CameraConstants* camera_constants = (CameraConstants*)world_const_buffers_.Mapped(pframe_index_);
  glm::mat4 view_proj = proj * view;
  camera_constants->time_and_res = time_and_res;  // TODO(cort): this belongs in a separate buffer
  camera_constants->eye_pos_ws = glm::vec4(ExtractViewPos(view), 1.0f);
  camera_constants->eye_dir_wsn = glm::normalize(glm::vec4(-view[0][2], -view[1][2], -view[2][2], 0));
  camera_constants->view_proj = view_proj;
  camera_constants->view = view;
  camera_constants->proj = proj;
  camera_constants->view_proj_inv = glm::inverse(view_proj);
  camera_constants->view_inv = glm::inverse(view);
  camera_constants->proj_inv = glm::inverse(proj);
  world_const_buffers_.FlushPframeHostCache(pframe_index_);

  VkDescriptorSet active_global_dset = VK_NULL_HANDLE;
  VkDescriptorSet active_material_dset = VK_NULL_HANDLE;
  VkPipeline active_pipeline = VK_NULL_HANDLE;
  const Mesh* active_mesh = nullptr;
  for (const auto& instance : instances_) {
    // Bind pipeline
    if (instance.material_->pipeline->handle != active_pipeline) {
      active_pipeline = instance.material_->pipeline->handle;
      vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, active_pipeline);
    }
    // Bind global dset
    if (active_global_dset != global_dsets_[pframe_index_]) {
      active_global_dset = global_dsets_[pframe_index_];
      vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
          instance.material_->pipeline->shader_program->pipeline_layout, 0, 1, &active_global_dset, 0, nullptr);
    }
    // Bind material dset
    if (active_material_dset != instance.material_->material_dsets[pframe_index_]) {
#if 0
      active_material_dset = instance.material_->material_dsets[pframe_index_];
      vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
          instance.material_->pipeline->shader_program->pipeline_layout, 1, 1, &active_material_dset, 0, nullptr);
#endif
    }
    // Bind mesh
    if (instance.mesh_ != active_mesh) {
      active_mesh = instance.mesh_;
      active_mesh->BindBuffers(cb);
    }
    // Bind instance dset
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
        instance.material_->pipeline->shader_program->pipeline_layout, 2, 1, &(instance.instance_dsets[pframe_index_]),
        0, nullptr);
    // Draw
    vkCmdDrawIndexed(cb, active_mesh->index_count, 1, 0, 0, 0);
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
     - bind instance dset
     - vkCmdDraw
2. render (clever)
   - bind world/camera const dset
   - for each material bucket:
     - bind material pipeline
     - bind material dset
     - for each mesh bucket:
       - bind mesh VB/IB
       - for each active/visible instance of this mesh:
         - bind instance dset
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
}  // namespace spokk
