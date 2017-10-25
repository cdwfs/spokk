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
  glm::vec4 time_and_res;  // x: elapsed seconds, yz: viewport resolution in pixels
  glm::vec4 eye_pos_ws;  // xyz: world-space eye position
  glm::vec4 eye_dir_wsn;  // xyz: world-space eye direction (normalized)
  glm::mat4 viewproj;
  glm::mat4 view;
  glm::mat4 proj;
  glm::mat4 viewproj_inv;
  glm::mat4 view_inv;
  glm::mat4 proj_inv;
};
struct MeshUniforms {
  glm::mat4 o2w;
};
struct MaterialUniforms {
  glm::vec4 albedo;  // xyz: albedo RGB
  glm::vec4 emissive_color;  // xyz: emissive color, w: intensity
  glm::vec4 spec_color;  // xyz: specular color, w: intensity
  glm::vec4 spec_exp;  // x: specular exponent
};
struct LightUniforms {
  glm::vec4 hemi_down_color;
  glm::vec4 hemi_up_color;

  glm::vec4 dir_color;  // xyz: color, w: intensity
  glm::vec4 dir_to_light_wsn;  // xyz: world-space normalized vector towards light

  glm::vec4 point_pos_ws_inverse_range;  // xyz: world-space light pos, w: inverse range of light
  glm::vec4 point_color;  // xyz: color, w: intensity

  glm::vec4 spot_pos_ws_inverse_range;  // xyz: world-space light pos, w: inverse range of light
  glm::vec4 spot_color;  // xyz: RGB color, w: intensity
  glm::vec4 spot_neg_dir_wsn;  // xyz: world-space normalized light direction (negated)
  glm::vec4 spot_falloff_angles;  // x: 1/(cos(inner)-cos(outer)), y: cos(outer)
};
constexpr float FOV_DEGREES = 45.0f;
constexpr float Z_NEAR = 0.01f;
constexpr float Z_FAR = 100.0f;

void EncodeSpotlightFalloffAngles(glm::vec4* out_encoded, float inner_degrees, float outer_degrees) {
  float cos_inner = cosf(((float)M_PI / 180.0f) * inner_degrees);
  float cos_outer = cosf(((float)M_PI / 180.0f) * outer_degrees);
  *out_encoded = glm::vec4(1.0f / (cos_inner - cos_outer), cos_outer, 0.0f, 0.0f);
}
void DecodeSpotlightFalloffAngles(float* out_inner_degrees, float* out_outer_degrees, glm::vec4 encoded) {
  float cos_outer = encoded.y;
  float cos_inner = (1.0f / encoded.x) + cos_outer;
  *out_outer_degrees = (180.0f / (float)M_PI) * acosf(cos_outer);
  *out_inner_degrees = (180.0f / (float)M_PI) * acosf(cos_inner);
}

}  // namespace

class LightsApp : public spokk::Application {
public:
  explicit LightsApp(Application::CreateInfo& ci) : Application(ci) {
    seconds_elapsed_ = 0;

    camera_ =
        my_make_unique<CameraPersp>(swapchain_extent_.width, swapchain_extent_.height, FOV_DEGREES, Z_NEAR, Z_FAR);
    const glm::vec3 initial_camera_pos(-3.63f, 4.27f, 9.17f);
    const glm::vec3 initial_camera_target(0, 3, 0);
    const glm::vec3 initial_camera_up(0, 1, 0);
    camera_->lookAt(initial_camera_pos, initial_camera_target, initial_camera_up);
    drone_ = my_make_unique<CameraDrone>(*camera_);

    // Create render pass
    render_pass_.InitFromPreset(RenderPass::Preset::COLOR_DEPTH, swapchain_surface_format_.format);
    SPOKK_VK_CHECK(render_pass_.Finalize(device_));
    render_pass_.clear_values[0] = CreateColorClearValue(0.2f, 0.2f, 0.3f);
    render_pass_.clear_values[1] = CreateDepthClearValue(1.0f, 0);

    // Initialize IMGUI
    InitImgui(render_pass_);
    ShowImgui(false);

    // Load textures and samplers
    VkSamplerCreateInfo sampler_ci =
        GetSamplerCreateInfo(VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);
    SPOKK_VK_CHECK(vkCreateSampler(device_, &sampler_ci, host_allocator_, &sampler_));
    int load_error = skybox_tex_.CreateFromFile(device_, graphics_and_present_queue_, "data/sanfrancisco4-512.ktx");
    ZOMBO_ASSERT(load_error == 0, "texture load error (%d)", load_error);

    // Load shaders (forcing compatible pipeline layouts)
    SPOKK_VK_CHECK(skybox_vs_.CreateAndLoadSpirvFile(device_, "data/skybox.vert.spv"));
    SPOKK_VK_CHECK(skybox_fs_.CreateAndLoadSpirvFile(device_, "data/skybox.frag.spv"));
    SPOKK_VK_CHECK(skybox_shader_program_.AddShader(&skybox_vs_));
    SPOKK_VK_CHECK(skybox_shader_program_.AddShader(&skybox_fs_));
    SPOKK_VK_CHECK(mesh_vs_.CreateAndLoadSpirvFile(device_, "data/lit_mesh.vert.spv"));
    SPOKK_VK_CHECK(mesh_fs_.CreateAndLoadSpirvFile(device_, "data/lit_mesh.frag.spv"));
    SPOKK_VK_CHECK(mesh_shader_program_.AddShader(&mesh_vs_));
    SPOKK_VK_CHECK(mesh_shader_program_.AddShader(&mesh_fs_));
    SPOKK_VK_CHECK(
        ShaderProgram::ForceCompatibleLayoutsAndFinalize(device_, {&skybox_shader_program_, &mesh_shader_program_}));

    // Create skybox pipeline
    empty_mesh_format_.Finalize(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    skybox_pipeline_.Init(&empty_mesh_format_, &skybox_shader_program_, &render_pass_, 0);
    skybox_pipeline_.depth_stencil_state_ci.depthWriteEnable = VK_FALSE;
    skybox_pipeline_.depth_stencil_state_ci.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    SPOKK_VK_CHECK(skybox_pipeline_.Finalize(device_));

    // Populate Mesh object
    int mesh_load_error = mesh_.CreateFromFile(device_, "data/teapot.mesh");
    ZOMBO_ASSERT(!mesh_load_error, "Error loading mesh");

    // Create mesh pipeline
    mesh_pipeline_.Init(&mesh_.mesh_format, &mesh_shader_program_, &render_pass_, 0);
    SPOKK_VK_CHECK(mesh_pipeline_.Finalize(device_));

    // Create pipelined buffer of light uniforms
    VkBufferCreateInfo light_uniforms_ci = {};
    light_uniforms_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    light_uniforms_ci.size = sizeof(LightUniforms);
    light_uniforms_ci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    light_uniforms_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    SPOKK_VK_CHECK(
        light_uniforms_.Create(device_, PFRAME_COUNT, light_uniforms_ci, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
    // Default light settings
    lights_.hemi_down_color = glm::vec4(0.471f, 0.412f, 0.282f, 0.75f);
    lights_.hemi_up_color = glm::vec4(0.290f, 0.390f, 0.545f, 0.0f);
    lights_.dir_to_light_wsn = glm::vec4(-1.0f, +1.0f, +1.0f, 0.0f);
    lights_.dir_color = glm::vec4(1.000f, 1.000f, 1.000f, 0.5f);
    lights_.point_pos_ws_inverse_range = glm::vec4(+0.000f, +0.000f, -5.000f, 1.0f / 10000.0f);
    lights_.point_color = glm::vec4(0.000f, 0.000f, 1.000f, 0.0f);
    lights_.spot_pos_ws_inverse_range = glm::vec4(-10.0f, 0.0f, 0.0f, 1.0 / 1000.0f);
    lights_.spot_color = glm::vec4(0.3f, 1.0f, 0.2f, 0.4f);
    lights_.spot_neg_dir_wsn = -glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
    EncodeSpotlightFalloffAngles(&lights_.spot_falloff_angles, 10.0f, 20.0f);

    // Create pipelined buffer of light uniforms
    VkBufferCreateInfo material_uniforms_ci = {};
    material_uniforms_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    material_uniforms_ci.size = sizeof(MaterialUniforms);
    material_uniforms_ci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    material_uniforms_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    SPOKK_VK_CHECK(
        material_uniforms_.Create(device_, PFRAME_COUNT, material_uniforms_ci, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
    material_.albedo = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    material_.emissive_color = glm::vec4(0.5f, 0.5f, 0.0f, 0.0f);
    material_.spec_color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    material_.spec_exp = glm::vec4(1000.0f, 0.0f, 0.0f, 0.0f);

    // Create pipelined buffer of mesh uniforms
    VkBufferCreateInfo mesh_uniforms_ci = {};
    mesh_uniforms_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    mesh_uniforms_ci.size = sizeof(MeshUniforms);
    mesh_uniforms_ci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    mesh_uniforms_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    SPOKK_VK_CHECK(mesh_uniforms_.Create(device_, PFRAME_COUNT, mesh_uniforms_ci, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));

    // Create pipelined buffer of shader uniforms
    VkBufferCreateInfo scene_uniforms_ci = {};
    scene_uniforms_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    scene_uniforms_ci.size = sizeof(SceneUniforms);
    scene_uniforms_ci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    scene_uniforms_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    SPOKK_VK_CHECK(
        scene_uniforms_.Create(device_, PFRAME_COUNT, scene_uniforms_ci, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));

    for (const auto& dset_layout_ci : skybox_shader_program_.dset_layout_cis) {
      dpool_.Add(dset_layout_ci, PFRAME_COUNT);
    }
    SPOKK_VK_CHECK(dpool_.Finalize(device_));
    for (uint32_t pframe = 0; pframe < PFRAME_COUNT; ++pframe) {
      // TODO(cort): allocate_pipelined_set()?
      dsets_[pframe] = dpool_.AllocateSet(device_, skybox_shader_program_.dset_layouts[0]);
    }
    DescriptorSetWriter dset_writer(skybox_shader_program_.dset_layout_cis[0]);
    dset_writer.BindImage(skybox_tex_.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        skybox_fs_.GetDescriptorBindPoint("skybox_tex").binding);
    dset_writer.BindSampler(sampler_, skybox_fs_.GetDescriptorBindPoint("skybox_samp").binding);
    for (uint32_t pframe = 0; pframe < PFRAME_COUNT; ++pframe) {
      dset_writer.BindBuffer(scene_uniforms_.Handle(pframe), mesh_vs_.GetDescriptorBindPoint("scene_consts").binding);
      dset_writer.BindBuffer(mesh_uniforms_.Handle(pframe), mesh_vs_.GetDescriptorBindPoint("mesh_consts").binding);
      dset_writer.BindBuffer(light_uniforms_.Handle(pframe), mesh_fs_.GetDescriptorBindPoint("light_consts").binding);
      dset_writer.BindBuffer(material_uniforms_.Handle(pframe), mesh_fs_.GetDescriptorBindPoint("mat_consts").binding);
      dset_writer.WriteAll(device_, dsets_[pframe]);
    }

    // Create swapchain-sized buffers
    CreateRenderBuffers(swapchain_extent_);
  }
  virtual ~LightsApp() {
    if (device_) {
      vkDeviceWaitIdle(device_);

      dpool_.Destroy(device_);

      light_uniforms_.Destroy(device_);
      material_uniforms_.Destroy(device_);
      mesh_uniforms_.Destroy(device_);
      scene_uniforms_.Destroy(device_);

      mesh_vs_.Destroy(device_);
      mesh_fs_.Destroy(device_);
      mesh_shader_program_.Destroy(device_);
      mesh_pipeline_.Destroy(device_);
      mesh_.Destroy(device_);

      skybox_vs_.Destroy(device_);
      skybox_fs_.Destroy(device_);
      skybox_shader_program_.Destroy(device_);
      skybox_pipeline_.Destroy(device_);

      vkDestroySampler(device_, sampler_, host_allocator_);
      skybox_tex_.Destroy(device_);

      for (const auto fb : framebuffers_) {
        vkDestroyFramebuffer(device_, fb, host_allocator_);
      }
      render_pass_.Destroy(device_);

      depth_image_.Destroy(device_);
    }
  }

  LightsApp(const LightsApp&) = delete;
  const LightsApp& operator=(const LightsApp&) = delete;

  virtual void Update(double dt) override {
    seconds_elapsed_ += dt;
    drone_->Update(input_state_, (float)dt);

    // Tweakable light/material settings
    const ImGuiColorEditFlags default_color_edit_flags = ImGuiColorEditFlags_Float | ImGuiColorEditFlags_PickerHueWheel;
    if (ImGui::TreeNode("Material")) {
      ImGui::ColorEdit3("Albedo", &material_.albedo.x, default_color_edit_flags);
      ImGui::Separator();
      ImGui::Text("Emissive:");
      ImGui::ColorEdit3("Color##Emissive", &material_.emissive_color.x, default_color_edit_flags);
      ImGui::SliderFloat("Intensity##Emissive", &material_.emissive_color.w, 0.0f, 1.0f);
      ImGui::Separator();
      ImGui::Text("Specular:");
      ImGui::ColorEdit3("Color##Spec", &material_.spec_color.x, default_color_edit_flags);
      ImGui::SliderFloat("Intensity##Spec", &material_.spec_color.w, 0.0f, 1.0f);
      ImGui::SliderFloat("Exponent##Spec", &material_.spec_exp.x, 1.0f, 100000.0f, "%.2f", 10.0f);
      ImGui::TreePop();
    }
    if (ImGui::TreeNode("Lights")) {
      ImGui::Text("Hemi Light");
      ImGui::ColorEdit3("Up Color##Hemi", &lights_.hemi_up_color.x, default_color_edit_flags);
      ImGui::ColorEdit3("Down Color##Hemi", &lights_.hemi_down_color.x, default_color_edit_flags);
      ImGui::SliderFloat("Intensity##Hemi", &lights_.hemi_down_color.w, 0.0f, 1.0f);

      ImGui::Separator();
      ImGui::Text("Dir Light:");
      ImGui::ColorEdit3("Color##Dir", &lights_.dir_color.x, default_color_edit_flags);
      ImGui::SliderFloat("Intensity##Dir", &lights_.dir_color.w, 0.0f, 1.0f);

      ImGui::Separator();
      ImGui::Text("Point Light:");
      float point_range = 1.0f / lights_.point_pos_ws_inverse_range.w;
      ImGui::InputFloat3("Position##Point", &lights_.point_pos_ws_inverse_range.x);
      ImGui::SliderFloat("Range##Point", &point_range, 0.001f, 1000000.0f, "%.3f", 10.0f);
      ImGui::ColorEdit3("Color##Point", &lights_.point_color.x, default_color_edit_flags);
      ImGui::SliderFloat("Intensity##Point", &lights_.point_color.w, 0.0f, 1.0f);
      lights_.point_pos_ws_inverse_range.w = 1.0f / point_range;

      ImGui::Separator();
      ImGui::Text("Spot Light:");
      float spot_range = 1.0f / lights_.spot_pos_ws_inverse_range.w;
      glm::vec3 spot_dir_wsn = -lights_.spot_neg_dir_wsn;
      float spot_falloff_degrees_outer = 0, spot_falloff_degrees_inner = 0;
      DecodeSpotlightFalloffAngles(
        &spot_falloff_degrees_inner, &spot_falloff_degrees_outer, lights_.spot_falloff_angles);
      float spot_falloff_degrees_inner_original = spot_falloff_degrees_inner;
      float spot_falloff_degrees_outer_original = spot_falloff_degrees_outer;
      ImGui::InputFloat3("Position##Spot", &lights_.spot_pos_ws_inverse_range.x);
      ImGui::SliderFloat("Range##Spot", &spot_range, 0.001f, 1000000.0f, "%.3f", 10.0f);
      ImGui::ColorEdit3("Color##Spot", &lights_.spot_color.x, default_color_edit_flags);
      ImGui::SliderFloat("Intensity##Spot", &lights_.spot_color.w, 0.0f, 1.0f);
      ImGui::InputFloat3("Direction##Spot", &spot_dir_wsn.x, 3);
      ImGui::SliderFloat("Inner Angle##Spot", &spot_falloff_degrees_inner, 0.0f, 90.0f);
      ImGui::SliderFloat("Outer Angle##Spot", &spot_falloff_degrees_outer, 0.0f, 90.0f);
      lights_.point_pos_ws_inverse_range.w = 1.0f / spot_range;
      lights_.spot_neg_dir_wsn = glm::vec4(-spot_dir_wsn, 0.0f);
      if (spot_falloff_degrees_inner != spot_falloff_degrees_inner_original &&
        spot_falloff_degrees_inner > spot_falloff_degrees_outer) {
        spot_falloff_degrees_outer = spot_falloff_degrees_inner;
      } else if (spot_falloff_degrees_outer != spot_falloff_degrees_outer_original &&
        spot_falloff_degrees_outer < spot_falloff_degrees_inner) {
        spot_falloff_degrees_inner = spot_falloff_degrees_outer;
      }
      EncodeSpotlightFalloffAngles(
        &lights_.spot_falloff_angles, spot_falloff_degrees_inner, spot_falloff_degrees_outer);

      ImGui::TreePop();
    }
  }

  void Render(VkCommandBuffer primary_cb, uint32_t swapchain_image_index) override {
    // Update uniforms
    SceneUniforms* scene_uniforms = (SceneUniforms*)scene_uniforms_.Mapped(pframe_index_);
    scene_uniforms->time_and_res =
      glm::vec4((float)seconds_elapsed_, (float)swapchain_extent_.width, (float)swapchain_extent_.height, 0);
    scene_uniforms->eye_pos_ws = glm::vec4(camera_->getEyePoint(), 1.0f);
    scene_uniforms->eye_dir_wsn = glm::vec4(glm::normalize(camera_->getViewDirection()), 1.0f);
    const glm::mat4 view = camera_->getViewMatrix();
    const glm::mat4 proj = camera_->getProjectionMatrix();
    const glm::mat4 viewproj = proj * view;
    scene_uniforms->viewproj = viewproj;
    scene_uniforms->view = view;
    scene_uniforms->proj = proj;
    scene_uniforms->viewproj_inv = glm::inverse(viewproj);
    scene_uniforms->view_inv = glm::inverse(view);
    scene_uniforms->proj_inv = glm::inverse(proj);
    scene_uniforms_.FlushPframeHostCache(pframe_index_);

    // Update mesh uniforms
    MeshUniforms* mesh_uniforms = (MeshUniforms*)mesh_uniforms_.Mapped(pframe_index_);
    mesh_uniforms->o2w = ComposeTransform(glm::vec3(0.0f, 0.0f, 0.0f), glm::quat_identity<float, glm::highp>(), 5.0f);
    mesh_uniforms_.FlushPframeHostCache(pframe_index_);

    // Update material uniforms
    MaterialUniforms* material_uniforms = (MaterialUniforms*)material_uniforms_.Mapped(pframe_index_);
    *material_uniforms = material_;
    material_uniforms_.FlushPframeHostCache(pframe_index_);

    // Update light uniforms
    LightUniforms* light_uniforms = (LightUniforms*)light_uniforms_.Mapped(pframe_index_);
    *light_uniforms = lights_;
    light_uniforms_.FlushPframeHostCache(pframe_index_);

    // Write command buffer
    VkFramebuffer framebuffer = framebuffers_[swapchain_image_index];
    render_pass_.begin_info.framebuffer = framebuffer;
    render_pass_.begin_info.renderArea.extent = swapchain_extent_;
    vkCmdBeginRenderPass(primary_cb, &render_pass_.begin_info, VK_SUBPASS_CONTENTS_INLINE);
    // Set up shared render state
    VkRect2D scissor_rect = render_pass_.begin_info.renderArea;
    VkViewport viewport = Rect2DToViewport(scissor_rect);
    vkCmdSetViewport(primary_cb, 0, 1, &viewport);
    vkCmdSetScissor(primary_cb, 0, 1, &scissor_rect);
    vkCmdBindDescriptorSets(primary_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline_.shader_program->pipeline_layout,
        0, 1, &dsets_[pframe_index_], 0, nullptr);
    // Render scene
    vkCmdBindPipeline(primary_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline_.handle);
    mesh_.BindBuffers(primary_cb);
    vkCmdDrawIndexed(primary_cb, mesh_.index_count, 1, 0, 0, 0);
    // Render skybox
    vkCmdBindPipeline(primary_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, skybox_pipeline_.handle);
    vkCmdDraw(primary_cb, 36, 1, 0, 0);
    RenderImgui(primary_cb);
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

  Image skybox_tex_;
  VkSampler sampler_;

  Shader skybox_vs_, skybox_fs_;
  ShaderProgram skybox_shader_program_;
  GraphicsPipeline skybox_pipeline_;
  MeshFormat empty_mesh_format_;

  DescriptorPool dpool_;
  std::array<VkDescriptorSet, PFRAME_COUNT> dsets_;

  Shader mesh_vs_, mesh_fs_;
  ShaderProgram mesh_shader_program_;
  GraphicsPipeline mesh_pipeline_;
  Mesh mesh_;
  PipelinedBuffer light_uniforms_;
  PipelinedBuffer material_uniforms_;
  PipelinedBuffer mesh_uniforms_;
  PipelinedBuffer scene_uniforms_;

  LightUniforms lights_;
  MaterialUniforms material_;

  std::unique_ptr<CameraPersp> camera_;
  std::unique_ptr<CameraDrone> drone_;
};

int main(int argc, char* argv[]) {
  (void)argc;
  (void)argv;

  std::vector<Application::QueueFamilyRequest> queue_requests = {
      {(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT), true, 1, 0.0f}};
  Application::CreateInfo app_ci = {};
  app_ci.queue_family_requests = queue_requests;
  app_ci.pfn_set_device_features = EnableMinimumDeviceFeatures;

  LightsApp app(app_ci);
  int run_error = app.Run();

  return run_error;
}
