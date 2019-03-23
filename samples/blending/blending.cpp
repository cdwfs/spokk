#include <spokk.h>
using namespace spokk;

#include <common/camera.h>
#include <common/cube_mesh.h>

#include <imgui.h>

#include <array>
#include <cstdio>
#include <memory>

namespace {
struct SceneUniforms {
  glm::vec4 res_and_time;  // xy: viewport resolution in pixels, z: unused, w: elapsed seconds
  glm::vec4 eye;  // xyz: eye position
  glm::mat4 viewproj;
};
struct MeshUniforms {
  glm::mat4 o2w;
  glm::vec4 albedo;  // xyz=color, w=opacity
  glm::vec4 spec_params;  // x=exponent, y=intensity
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
    SPOKK_VK_CHECK(mesh_vs_.CreateAndLoadSpirvFile(device_, "data/blending/dsb_mesh.vert.spv"));
    SPOKK_VK_CHECK(mesh_fs_.CreateAndLoadSpirvFile(device_, "data/blending/dsb_mesh.frag.spv"));
    SPOKK_VK_CHECK(mesh_shader_program_.AddShader(&mesh_vs_));
    SPOKK_VK_CHECK(mesh_shader_program_.AddShader(&mesh_fs_));
    SPOKK_VK_CHECK(mesh_shader_program_.Finalize(device_));

    // Populate Mesh objects
    int mesh_load_error = bg_mesh_.CreateFromFile(device_, "data/cube.mesh");
    ZOMBO_ASSERT(!mesh_load_error, "load error: %d", mesh_load_error);
    mesh_load_error = fg_mesh_.CreateFromFile(device_, "data/teapot.mesh");
    ZOMBO_ASSERT(!mesh_load_error, "load error: %d", mesh_load_error);

    mesh_pipeline_.Init(&fg_mesh_.mesh_format, &mesh_shader_program_, &render_pass_, 0);
    mesh_pipeline_.color_blend_attachment_states[0].blendEnable = VK_TRUE;
    mesh_pipeline_.color_blend_attachment_states[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    mesh_pipeline_.color_blend_attachment_states[0].dstColorBlendFactor = VK_BLEND_FACTOR_SRC1_COLOR;
    mesh_pipeline_.color_blend_attachment_states[0].colorBlendOp = VK_BLEND_OP_ADD;
    mesh_pipeline_.color_blend_attachment_states[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    mesh_pipeline_.color_blend_attachment_states[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    mesh_pipeline_.color_blend_attachment_states[0].alphaBlendOp = VK_BLEND_OP_ADD;
    SPOKK_VK_CHECK(mesh_pipeline_.Finalize(device_));
    SPOKK_VK_CHECK(device_.SetObjectName(mesh_pipeline_.handle, "mesh pipeline"));

    for (const auto& dset_layout_ci : mesh_shader_program_.dset_layout_cis) {
      dpool_.Add(dset_layout_ci, PFRAME_COUNT);  // for bg mesh
      dpool_.Add(dset_layout_ci, PFRAME_COUNT);  // for fg mesh
    }
    SPOKK_VK_CHECK(dpool_.Finalize(device_));

    // Look up the appropriate memory flags for uniform buffers on this platform
    VkMemoryPropertyFlags uniform_buffer_memory_flags =
        device_.MemoryFlagsForAccessPattern(DEVICE_MEMORY_ACCESS_PATTERN_CPU_TO_GPU_DYNAMIC);

    DescriptorSetWriter dset_writer(mesh_shader_program_.dset_layout_cis[0]);
    for (uint32_t pframe = 0; pframe < PFRAME_COUNT; ++pframe) {
      auto& frame_data = frame_data_[pframe];
      // Create per-pframe buffer of shader uniforms
      VkBufferCreateInfo scene_uniforms_ci = {};
      scene_uniforms_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
      scene_uniforms_ci.size = sizeof(SceneUniforms);
      scene_uniforms_ci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
      scene_uniforms_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
      SPOKK_VK_CHECK(frame_data.scene_ubo.Create(device_, scene_uniforms_ci, uniform_buffer_memory_flags));
      SPOKK_VK_CHECK(device_.SetObjectName(frame_data.scene_ubo.Handle(),
          "scene uniform buffer " + std::to_string(pframe)));  // TODO(cort): absl::StrCat
      dset_writer.BindBuffer(frame_data.scene_ubo.Handle(), mesh_vs_.GetDescriptorBindPoint("scene_consts").binding);

      // Create per-pframe buffer of per-mesh uniforms
      VkBufferCreateInfo o2w_buffer_ci = {};
      o2w_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
      o2w_buffer_ci.size = sizeof(MeshUniforms);
      o2w_buffer_ci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
      o2w_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
      SPOKK_VK_CHECK(frame_data.bg_mesh_ubo.Create(device_, o2w_buffer_ci, uniform_buffer_memory_flags));
      SPOKK_VK_CHECK(frame_data.fg_mesh_ubo.Create(device_, o2w_buffer_ci, uniform_buffer_memory_flags));
      SPOKK_VK_CHECK(device_.SetObjectName(frame_data.bg_mesh_ubo.Handle(),
          "bg mesh uniform buffer " + std::to_string(pframe)));  // TODO(cort): absl::StrCat
      SPOKK_VK_CHECK(device_.SetObjectName(frame_data.fg_mesh_ubo.Handle(),
          "fg mesh uniform buffer " + std::to_string(pframe)));  // TODO(cort): absl::StrCat

      frame_data.bg_dset = dpool_.AllocateSet(device_, mesh_shader_program_.dset_layouts[0]);
      frame_data.fg_dset = dpool_.AllocateSet(device_, mesh_shader_program_.dset_layouts[0]);
      SPOKK_VK_CHECK(device_.SetObjectName(
          frame_data.bg_dset, "bg frame dset " + std::to_string(pframe)));  // TODO(cort): absl::StrCat
      SPOKK_VK_CHECK(device_.SetObjectName(
          frame_data.fg_dset, "fg frame dset " + std::to_string(pframe)));  // TODO(cort): absl::StrCat

      dset_writer.BindBuffer(frame_data.bg_mesh_ubo.Handle(), mesh_vs_.GetDescriptorBindPoint("mesh_consts").binding);
      dset_writer.WriteAll(device_, frame_data.bg_dset);
      dset_writer.BindBuffer(frame_data.fg_mesh_ubo.Handle(), mesh_vs_.GetDescriptorBindPoint("mesh_consts").binding);
      dset_writer.WriteAll(device_, frame_data.fg_dset);
    }

    // Create swapchain-sized buffers
    CreateRenderBuffers(swapchain_extent_);
  }
  virtual ~BlendingApp() {
    if (device_ != VK_NULL_HANDLE) {
      vkDeviceWaitIdle(device_);

      dpool_.Destroy(device_);

      for (auto& frame_data : frame_data_) {
        frame_data.fg_mesh_ubo.Destroy(device_);
        frame_data.bg_mesh_ubo.Destroy(device_);
        frame_data.scene_ubo.Destroy(device_);
      }

      fg_mesh_.Destroy(device_);
      bg_mesh_.Destroy(device_);

      mesh_vs_.Destroy(device_);
      mesh_fs_.Destroy(device_);
      mesh_shader_program_.Destroy(device_);
      mesh_pipeline_.Destroy(device_);

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

    const ImGuiColorEditFlags default_color_edit_flags = ImGuiColorEditFlags_Float |
        ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview;
    ImGui::Text("Background Mesh");
    ImGui::ColorEdit4("Albedo##BG", &bg_mesh_albedo_.x, default_color_edit_flags);
    ImGui::SliderFloat("Spec Exp##BG", &bg_mesh_spec_exponent_, 1.0f, 100000.0f, "%.2f", 10.0f);
    ImGui::SliderFloat("Spec Intensity##BG", &bg_mesh_spec_intensity_, 0.0f, 1.0f);
    ImGui::Separator();
    ImGui::Text("Foreground Mesh");
    ImGui::ColorEdit4("Albedo##FG", &fg_mesh_albedo_.x, default_color_edit_flags);
    ImGui::SliderFloat("Spec Exp##FG", &fg_mesh_spec_exponent_, 1.0f, 100000.0f, "%.2f", 10.0f);
    ImGui::SliderFloat("Spec Intensity##FG", &fg_mesh_spec_intensity_, 0.0f, 1.0f);
  }

  void Render(VkCommandBuffer primary_cb, uint32_t swapchain_image_index) override {
    // Update uniforms
    SceneUniforms* scene_uniforms = (SceneUniforms*)frame_data_[pframe_index_].scene_ubo.Mapped();
    scene_uniforms->res_and_time =
        glm::vec4((float)swapchain_extent_.width, (float)swapchain_extent_.height, 0, (float)seconds_elapsed_);
    scene_uniforms->eye = glm::vec4(camera_->getEyePoint(), 1.0f);
    glm::mat4 w2v = camera_->getViewMatrix();
    const glm::mat4 proj = camera_->getProjectionMatrix();
    scene_uniforms->viewproj = proj * w2v;
    SPOKK_VK_CHECK(frame_data_[pframe_index_].scene_ubo.FlushHostCache(device_));

    // Update mesh uniforms
    const float secs = (float)seconds_elapsed_;
    MeshUniforms* bg_mesh_uniforms = (MeshUniforms*)frame_data_[pframe_index_].bg_mesh_ubo.Mapped();
    // clang-format off
    bg_mesh_uniforms->o2w = ComposeTransform(
      glm::vec3(sinf(0.2f * secs), 0.0f, -5.0f),
      glm::angleAxis(0.0f, glm::vec3(0,1,0)),
      1.0f);
    bg_mesh_uniforms->albedo = bg_mesh_albedo_;
    bg_mesh_uniforms->spec_params.x = bg_mesh_spec_exponent_;
    bg_mesh_uniforms->spec_params.y = bg_mesh_spec_intensity_;
    // clang-format on
    SPOKK_VK_CHECK(frame_data_[pframe_index_].bg_mesh_ubo.FlushHostCache(device_));

    MeshUniforms* fg_mesh_uniforms = (MeshUniforms*)frame_data_[pframe_index_].fg_mesh_ubo.Mapped();
    // clang-format off
    fg_mesh_uniforms->o2w = ComposeTransform(
      glm::vec3(0.0f, 0.0f, -0.0f),
      glm::angleAxis(0.0f, glm::vec3(0,1,0)),
      1.0f);
    fg_mesh_uniforms->albedo = fg_mesh_albedo_;
    fg_mesh_uniforms->spec_params.x = fg_mesh_spec_exponent_;
    fg_mesh_uniforms->spec_params.y = fg_mesh_spec_intensity_;
    // clang-format on
    SPOKK_VK_CHECK(frame_data_[pframe_index_].fg_mesh_ubo.FlushHostCache(device_));

    // Write command buffer
    VkFramebuffer framebuffer = framebuffers_[swapchain_image_index];
    render_pass_.begin_info.framebuffer = framebuffer;
    render_pass_.begin_info.renderArea.extent = swapchain_extent_;
    vkCmdBeginRenderPass(primary_cb, &render_pass_.begin_info, VK_SUBPASS_CONTENTS_INLINE);
    VkRect2D scissor_rect = render_pass_.begin_info.renderArea;
    VkViewport viewport = Rect2DToViewport(scissor_rect);
    vkCmdSetViewport(primary_cb, 0, 1, &viewport);
    vkCmdSetScissor(primary_cb, 0, 1, &scissor_rect);
    vkCmdBindPipeline(primary_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline_.handle);

    vkCmdBindDescriptorSets(primary_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline_.shader_program->pipeline_layout,
        0, 1, &(frame_data_[pframe_index_].bg_dset), 0, nullptr);
    bg_mesh_.BindBuffers(primary_cb);
    vkCmdDrawIndexed(primary_cb, bg_mesh_.index_count, 1, 0, 0, 0);

    vkCmdBindDescriptorSets(primary_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline_.shader_program->pipeline_layout,
        0, 1, &(frame_data_[pframe_index_].fg_dset), 0, nullptr);
    fg_mesh_.BindBuffers(primary_cb);
    vkCmdDrawIndexed(primary_cb, fg_mesh_.index_count, 1, 0, 0, 0);

    vkCmdEndRenderPass(primary_cb);
  }

protected:
  void HandleWindowResize(VkExtent2D new_window_extent) override {
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

  Shader mesh_vs_, mesh_fs_;
  ShaderProgram mesh_shader_program_;
  GraphicsPipeline mesh_pipeline_;

  DescriptorPool dpool_;

  struct FrameData {
    Buffer bg_mesh_ubo;
    Buffer fg_mesh_ubo;
    Buffer scene_ubo;
    VkDescriptorSet bg_dset;
    VkDescriptorSet fg_dset;
  };
  std::array<FrameData, PFRAME_COUNT> frame_data_;

  glm::vec4 bg_mesh_albedo_ = glm::vec4(0.0, 0.5f, 0.5f, 1.0f);
  float bg_mesh_spec_exponent_ = 100.0f;
  float bg_mesh_spec_intensity_ = 1.0f;

  glm::vec4 fg_mesh_albedo_ = glm::vec4(1.0f, 0.5f, 0.2f, 0.2f);
  float fg_mesh_spec_exponent_ = 100.0f;
  float fg_mesh_spec_intensity_ = 1.0f;

  Mesh bg_mesh_;
  Mesh fg_mesh_;

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
