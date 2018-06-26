#include <spokk.h>
using namespace spokk;

#include <common/camera.h>
#include <common/cube_mesh.h>

#include <array>
#include <cstdio>
#include <memory>

namespace {
struct SceneUniforms {
  glm::vec4 time_and_res;  // x: elapsed seconds, yz: viewport resolution in pixels
  glm::vec4 eye;  // xyz: eye position
  glm::mat4 viewproj;
};
struct MeshUniforms {
  glm::mat4 o2w;
};
constexpr float FOV_DEGREES = 45.0f;
constexpr float Z_NEAR = 0.01f;
constexpr float Z_FAR = 100.0f;
}  // namespace

class BlendingApp : public spokk::Application {
public:
  explicit BlendingApp(Application::CreateInfo& ci) : Application(ci) {
    ZOMBO_ASSERT(device_.Properties().limits.maxFragmentDualSrcAttachments >= 1,
        "Must support at least one dual-src attachment");

    seconds_elapsed_ = 0;

    camera_ =
        my_make_unique<CameraPersp>(swapchain_extent_.width, swapchain_extent_.height, FOV_DEGREES, Z_NEAR, Z_FAR);
    const glm::vec3 initial_camera_pos(-1, 0, 6);
    const glm::vec3 initial_camera_target(0, 0, 0);
    const glm::vec3 initial_camera_up(0, 1, 0);
    camera_->lookAt(initial_camera_pos, initial_camera_target, initial_camera_up);
    drone_ = my_make_unique<CameraDrone>(*camera_);

    // Create render pass
    render_pass_.InitFromPreset(RenderPass::Preset::COLOR_DEPTH, swapchain_surface_format_.format);
    SPOKK_VK_CHECK(render_pass_.Finalize(device_));
    render_pass_.clear_values[0] = CreateColorClearValue(0.2f, 0.2f, 0.3f);
    render_pass_.clear_values[1] = CreateDepthClearValue(1.0f, 0);

    // Load shader pipelines
    SPOKK_VK_CHECK(opaque_mesh_vs_.CreateAndLoadSpirvFile(device_, "data/blending/opaque_mesh.vert.spv"));
    SPOKK_VK_CHECK(opaque_mesh_fs_.CreateAndLoadSpirvFile(device_, "data/blending/opaque_mesh.frag.spv"));
    SPOKK_VK_CHECK(opaque_mesh_shader_program_.AddShader(&opaque_mesh_vs_));
    SPOKK_VK_CHECK(opaque_mesh_shader_program_.AddShader(&opaque_mesh_fs_));

    SPOKK_VK_CHECK(glass_mesh_vs_.CreateAndLoadSpirvFile(device_, "data/blending/dsb_mesh.vert.spv"));
    SPOKK_VK_CHECK(glass_mesh_fs_.CreateAndLoadSpirvFile(device_, "data/blending/dsb_mesh.frag.spv"));
    SPOKK_VK_CHECK(glass_mesh_shader_program_.AddShader(&glass_mesh_vs_));
    SPOKK_VK_CHECK(glass_mesh_shader_program_.AddShader(&glass_mesh_fs_));

    std::vector<ShaderProgram*> all_shader_programs = {&opaque_mesh_shader_program_, &glass_mesh_shader_program_};
    SPOKK_VK_CHECK(ShaderProgram::ForceCompatibleLayoutsAndFinalize(device_, all_shader_programs));

    // Populate Mesh objects
    int mesh_load_error = opaque_mesh_.CreateFromFile(device_, "data/cube.mesh");
    ZOMBO_ASSERT(!mesh_load_error, "load error: %d", mesh_load_error);
    mesh_load_error = glass_mesh_.CreateFromFile(device_, "data/teapot.mesh");
    ZOMBO_ASSERT(!mesh_load_error, "load error: %d", mesh_load_error);

    // Look up the appropriate memory flags for uniform buffers on this platform
    VkMemoryPropertyFlags uniform_buffer_memory_flags =
        device_.MemoryFlagsForAccessPattern(DEVICE_MEMORY_ACCESS_PATTERN_CPU_TO_GPU_DYNAMIC);

    // Create pipelined buffer of per-mesh object-to-world matrices.
    VkBufferCreateInfo o2w_buffer_ci = {};
    o2w_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    o2w_buffer_ci.size = sizeof(glm::mat4);
    o2w_buffer_ci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    o2w_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    SPOKK_VK_CHECK(opaque_mesh_uniforms_.Create(device_, PFRAME_COUNT, o2w_buffer_ci, uniform_buffer_memory_flags));
    SPOKK_VK_CHECK(glass_mesh_uniforms_.Create(device_, PFRAME_COUNT, o2w_buffer_ci, uniform_buffer_memory_flags));

    // Create pipelined buffer of shader uniforms
    VkBufferCreateInfo scene_uniforms_ci = {};
    scene_uniforms_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    scene_uniforms_ci.size = sizeof(SceneUniforms);
    scene_uniforms_ci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    scene_uniforms_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    SPOKK_VK_CHECK(scene_uniforms_.Create(device_, PFRAME_COUNT, scene_uniforms_ci, uniform_buffer_memory_flags));

    opaque_mesh_pipeline_.Init(&opaque_mesh_.mesh_format, &opaque_mesh_shader_program_, &render_pass_, 0);
    SPOKK_VK_CHECK(opaque_mesh_pipeline_.Finalize(device_));
    glass_mesh_pipeline_.Init(&glass_mesh_.mesh_format, &glass_mesh_shader_program_, &render_pass_, 0);
    glass_mesh_pipeline_.color_blend_attachment_states[0].blendEnable = VK_TRUE;
    glass_mesh_pipeline_.color_blend_attachment_states[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    glass_mesh_pipeline_.color_blend_attachment_states[0].dstColorBlendFactor = VK_BLEND_FACTOR_SRC1_COLOR;
    glass_mesh_pipeline_.color_blend_attachment_states[0].colorBlendOp = VK_BLEND_OP_ADD;
    glass_mesh_pipeline_.color_blend_attachment_states[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    glass_mesh_pipeline_.color_blend_attachment_states[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    glass_mesh_pipeline_.color_blend_attachment_states[0].alphaBlendOp = VK_BLEND_OP_ADD;
    SPOKK_VK_CHECK(glass_mesh_pipeline_.Finalize(device_));

    for (const auto& dset_layout_ci : opaque_mesh_shader_program_.dset_layout_cis) {
      dpool_.Add(dset_layout_ci, PFRAME_COUNT);
    }
    for (const auto& dset_layout_ci : glass_mesh_shader_program_.dset_layout_cis) {
      dpool_.Add(dset_layout_ci, PFRAME_COUNT);
    }
    SPOKK_VK_CHECK(dpool_.Finalize(device_));
    for (uint32_t pframe = 0; pframe < PFRAME_COUNT; ++pframe) {
      // TODO(cort): allocate_pipelined_set()?
      dsets_[pframe] = dpool_.AllocateSet(device_, opaque_mesh_shader_program_.dset_layouts[0]);
    }
    DescriptorSetWriter dset_writer(opaque_mesh_shader_program_.dset_layout_cis[0]);
    for (uint32_t pframe = 0; pframe < PFRAME_COUNT; ++pframe) {
      dset_writer.BindBuffer(
          scene_uniforms_.Handle(pframe), opaque_mesh_vs_.GetDescriptorBindPoint("scene_consts").binding);
      dset_writer.BindBuffer(
          opaque_mesh_uniforms_.Handle(pframe), opaque_mesh_vs_.GetDescriptorBindPoint("opaque_mesh_consts").binding);
      dset_writer.BindBuffer(
          glass_mesh_uniforms_.Handle(pframe), glass_mesh_vs_.GetDescriptorBindPoint("glass_mesh_consts").binding);
      dset_writer.WriteAll(device_, dsets_[pframe]);
    }

    // Create swapchain-sized buffers
    CreateRenderBuffers(swapchain_extent_);
  }
  virtual ~BlendingApp() {
    if (device_ != VK_NULL_HANDLE) {
      vkDeviceWaitIdle(device_);

      dpool_.Destroy(device_);

      glass_mesh_uniforms_.Destroy(device_);
      opaque_mesh_uniforms_.Destroy(device_);
      scene_uniforms_.Destroy(device_);

      glass_mesh_.Destroy(device_);
      opaque_mesh_.Destroy(device_);

      glass_mesh_vs_.Destroy(device_);
      glass_mesh_fs_.Destroy(device_);
      glass_mesh_shader_program_.Destroy(device_);
      glass_mesh_pipeline_.Destroy(device_);
      opaque_mesh_vs_.Destroy(device_);
      opaque_mesh_fs_.Destroy(device_);
      opaque_mesh_shader_program_.Destroy(device_);
      opaque_mesh_pipeline_.Destroy(device_);

      for (const auto fb : framebuffers_) {
        vkDestroyFramebuffer(device_, fb, host_allocator_);
      }
      render_pass_.Destroy(device_);

      depth_image_.Destroy(device_);
    }
  }

  BlendingApp(const BlendingApp&) = delete;
  const BlendingApp& operator=(const BlendingApp&) = delete;

  virtual void Update(double dt) override {
    seconds_elapsed_ += dt;
    drone_->Update(input_state_, (float)dt);
  }

  void Render(VkCommandBuffer primary_cb, uint32_t swapchain_image_index) override {
    // Update uniforms
    SceneUniforms* uniforms = (SceneUniforms*)scene_uniforms_.Mapped(pframe_index_);
    uniforms->time_and_res =
        glm::vec4((float)seconds_elapsed_, (float)swapchain_extent_.width, (float)swapchain_extent_.height, 0);
    uniforms->eye = glm::vec4(camera_->getEyePoint(), 1.0f);
    glm::mat4 w2v = camera_->getViewMatrix();
    const glm::mat4 proj = camera_->getProjectionMatrix();
    uniforms->viewproj = proj * w2v;
    SPOKK_VK_CHECK(scene_uniforms_.FlushPframeHostCache(device_, pframe_index_));

    // Update object-to-world matrices.
    const float secs = (float)seconds_elapsed_;
    MeshUniforms* opaque_mesh_uniforms = (MeshUniforms*)opaque_mesh_uniforms_.Mapped(pframe_index_);
    // clang-format off
    opaque_mesh_uniforms->o2w = ComposeTransform(
      glm::vec3(sinf(0.2f * secs), 0.0f, -5.0f),
      glm::angleAxis(0.0f, glm::vec3(0,1,0)),
      1.0f);
    // clang-format on
    SPOKK_VK_CHECK(opaque_mesh_uniforms_.FlushPframeHostCache(device_, pframe_index_));

    MeshUniforms* glass_mesh_uniforms = (MeshUniforms*)glass_mesh_uniforms_.Mapped(pframe_index_);
    // clang-format off
    glass_mesh_uniforms->o2w = ComposeTransform(
      glm::vec3(0.0f, 0.0f, -0.0f),
      glm::angleAxis(0.0f, glm::vec3(0,1,0)),
      1.0f);
    // clang-format on
    SPOKK_VK_CHECK(glass_mesh_uniforms_.FlushPframeHostCache(device_, pframe_index_));

    // Write command buffer
    VkFramebuffer framebuffer = framebuffers_[swapchain_image_index];
    render_pass_.begin_info.framebuffer = framebuffer;
    render_pass_.begin_info.renderArea.extent = swapchain_extent_;
    vkCmdBeginRenderPass(primary_cb, &render_pass_.begin_info, VK_SUBPASS_CONTENTS_INLINE);
    VkRect2D scissor_rect = render_pass_.begin_info.renderArea;
    VkViewport viewport = Rect2DToViewport(scissor_rect);
    vkCmdSetViewport(primary_cb, 0, 1, &viewport);
    vkCmdSetScissor(primary_cb, 0, 1, &scissor_rect);
    vkCmdBindDescriptorSets(primary_cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
        opaque_mesh_pipeline_.shader_program->pipeline_layout, 0, 1, &dsets_[pframe_index_], 0, nullptr);

    vkCmdBindPipeline(primary_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, opaque_mesh_pipeline_.handle);
    opaque_mesh_.BindBuffers(primary_cb);
    vkCmdDrawIndexed(primary_cb, opaque_mesh_.index_count, 1, 0, 0, 0);

    vkCmdBindPipeline(primary_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, glass_mesh_pipeline_.handle);
    glass_mesh_.BindBuffers(primary_cb);
    vkCmdDrawIndexed(primary_cb, glass_mesh_.index_count, 1, 0, 0, 0);

    vkCmdEndRenderPass(primary_cb);
  }

protected:
  void HandleWindowResize(VkExtent2D new_window_extent) override {
    Application::HandleWindowResize(new_window_extent);

    // Destroy existing objects before re-creating them.
    for (auto fb : framebuffers_) {
      if (fb != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device_, fb, host_allocator_);
      }
    }
    framebuffers_.clear();
    depth_image_.Destroy(device_);

    float aspect_ratio = (float)new_window_extent.width / (float)new_window_extent.height;
    camera_->setPerspective(FOV_DEGREES, aspect_ratio, Z_NEAR, Z_FAR);

    CreateRenderBuffers(new_window_extent);
  }

private:
  void CreateRenderBuffers(VkExtent2D extent) {
    // Create depth buffer
    VkImageCreateInfo depth_image_ci = render_pass_.GetAttachmentImageCreateInfo(1, extent);
    depth_image_ = {};
    SPOKK_VK_CHECK(depth_image_.Create(
        device_, depth_image_ci, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DEVICE_ALLOCATION_SCOPE_DEVICE));

    // Create VkFramebuffers
    std::vector<VkImageView> attachment_views = {
        VK_NULL_HANDLE,  // filled in below
        depth_image_.view,
    };
    VkFramebufferCreateInfo framebuffer_ci = render_pass_.GetFramebufferCreateInfo(extent);
    framebuffer_ci.pAttachments = attachment_views.data();
    framebuffers_.resize(swapchain_image_views_.size());
    for (size_t i = 0; i < swapchain_image_views_.size(); ++i) {
      attachment_views[0] = swapchain_image_views_[i];
      SPOKK_VK_CHECK(vkCreateFramebuffer(device_, &framebuffer_ci, host_allocator_, &framebuffers_[i]));
    }
  }

  double seconds_elapsed_;

  Image depth_image_;

  RenderPass render_pass_;
  std::vector<VkFramebuffer> framebuffers_;

  Shader opaque_mesh_vs_, opaque_mesh_fs_;
  ShaderProgram opaque_mesh_shader_program_;
  GraphicsPipeline opaque_mesh_pipeline_;

  Shader glass_mesh_vs_, glass_mesh_fs_;
  ShaderProgram glass_mesh_shader_program_;
  GraphicsPipeline glass_mesh_pipeline_;

  DescriptorPool dpool_;
  std::array<VkDescriptorSet, PFRAME_COUNT> dsets_;

  Mesh opaque_mesh_;
  PipelinedBuffer opaque_mesh_uniforms_;

  Mesh glass_mesh_;
  PipelinedBuffer glass_mesh_uniforms_;

  PipelinedBuffer scene_uniforms_;

  std::unique_ptr<CameraPersp> camera_;
  std::unique_ptr<CameraDrone> drone_;
};

static VkBool32 EnableRequiredDeviceFeatures(
    const VkPhysicalDeviceFeatures& supported_features, VkPhysicalDeviceFeatures* enabled_features) {
  if (!supported_features.dualSrcBlend) {
    return VK_FALSE;
  }
  enabled_features->dualSrcBlend = VK_TRUE;

  return VK_TRUE;
}

int main(int argc, char* argv[]) {
  (void)argc;
  (void)argv;

  std::vector<Application::QueueFamilyRequest> queue_requests = {
      {(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT), true, 1, 0.0f}};
  Application::CreateInfo app_ci = {};
  app_ci.queue_family_requests = queue_requests;
  app_ci.pfn_set_device_features = EnableRequiredDeviceFeatures;

  BlendingApp app(app_ci);
  int run_error = app.Run();

  return run_error;
}
