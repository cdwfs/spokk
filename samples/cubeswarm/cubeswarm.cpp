#include <spokk.h>
using namespace spokk;

#include <common/camera.h>

#include <array>
#include <cstdio>
#include <memory>

namespace {
struct SceneUniforms {
  glm::vec4 res_and_time;  // xy: viewport resolution in pixels, z: unused, w: elapsed seconds
  glm::vec4 eye;  // xyz: eye position
  glm::mat4 viewproj;
};
constexpr uint32_t MESH_INSTANCE_COUNT = 1024;
struct MeshUniforms {
  glm::mat4 o2w[MESH_INSTANCE_COUNT];
};
constexpr float FOV_DEGREES = 45.0f;
constexpr float Z_NEAR = 0.01f;
constexpr float Z_FAR = 100.0f;
}  // namespace

class CubeSwarmApp : public spokk::Application {
public:
  explicit CubeSwarmApp(Application::CreateInfo& ci);
  virtual ~CubeSwarmApp();
  CubeSwarmApp(const CubeSwarmApp&) = delete;
  const CubeSwarmApp& operator=(const CubeSwarmApp&) = delete;

  virtual void Update(double dt) override;
  void Render(VkCommandBuffer primary_cb, uint32_t swapchain_image_index) override;

protected:
  void HandleWindowResize(VkExtent2D new_window_extent) override;

private:
  void CreateRenderBuffers(VkExtent2D extent);

  double seconds_elapsed_;

  Image depth_image_;

  RenderPass render_pass_;
  std::vector<VkFramebuffer> framebuffers_;

  Shader mesh_vs_, mesh_fs_;
  ShaderProgram mesh_shader_program_;
  GraphicsPipeline mesh_pipeline_;

  DescriptorPool dpool_;
  std::array<VkDescriptorSet, PFRAME_COUNT> dsets_;

  Mesh mesh_;
  PipelinedBuffer mesh_uniforms_;
  PipelinedBuffer scene_uniforms_;

  std::unique_ptr<CameraPersp> camera_;
  std::unique_ptr<CameraDrone> drone_;
};

CubeSwarmApp::CubeSwarmApp(Application::CreateInfo& ci) : Application(ci) {
  seconds_elapsed_ = 0;

  camera_ = my_make_unique<CameraPersp>(swapchain_extent_.width, swapchain_extent_.height, FOV_DEGREES, Z_NEAR, Z_FAR);
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
  device_.SetObjectName(render_pass_.handle, "Primary Render Pass");

  // Load shader pipelines
  SPOKK_VK_CHECK(mesh_vs_.CreateAndLoadSpirvFile(device_, "data/cubeswarm/rigid_mesh.vert.spv"));
  SPOKK_VK_CHECK(mesh_fs_.CreateAndLoadSpirvFile(device_, "data/cubeswarm/rigid_mesh.frag.spv"));
  SPOKK_VK_CHECK(mesh_shader_program_.AddShader(&mesh_vs_));
  SPOKK_VK_CHECK(mesh_shader_program_.AddShader(&mesh_fs_));
  SPOKK_VK_CHECK(mesh_shader_program_.Finalize(device_));

  // Populate Mesh object
  int mesh_load_error = mesh_.CreateFromFile(device_, "data/teapot.mesh");
  ZOMBO_ASSERT(!mesh_load_error, "load error: %d", mesh_load_error);

  // Look up the appropriate memory flags for uniform buffers on this platform
  VkMemoryPropertyFlags uniform_buffer_memory_flags =
      device_.MemoryFlagsForAccessPattern(DEVICE_MEMORY_ACCESS_PATTERN_CPU_TO_GPU_DYNAMIC);

  // Create pipelined buffer of per-mesh object-to-world matrices.
  VkBufferCreateInfo o2w_buffer_ci = {};
  o2w_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  o2w_buffer_ci.size = MESH_INSTANCE_COUNT * sizeof(glm::mat4);
  o2w_buffer_ci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  o2w_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  SPOKK_VK_CHECK(mesh_uniforms_.Create(device_, PFRAME_COUNT, o2w_buffer_ci, uniform_buffer_memory_flags));

  // Create pipelined buffer of shader uniforms
  VkBufferCreateInfo scene_uniforms_ci = {};
  scene_uniforms_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  scene_uniforms_ci.size = sizeof(SceneUniforms);
  scene_uniforms_ci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  scene_uniforms_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  SPOKK_VK_CHECK(scene_uniforms_.Create(device_, PFRAME_COUNT, scene_uniforms_ci, uniform_buffer_memory_flags));

  mesh_pipeline_.Init(&mesh_.mesh_format, &mesh_shader_program_, &render_pass_, 0);
  SPOKK_VK_CHECK(mesh_pipeline_.Finalize(device_));
  SPOKK_VK_CHECK(device_.SetObjectName(mesh_pipeline_.handle, "rigid mesh pipeline"));

  for (const auto& dset_layout_ci : mesh_shader_program_.dset_layout_cis) {
    dpool_.Add(dset_layout_ci, PFRAME_COUNT);
  }
  SPOKK_VK_CHECK(dpool_.Finalize(device_));
  for (uint32_t pframe = 0; pframe < PFRAME_COUNT; ++pframe) {
    // TODO(cort): allocate_pipelined_set()?
    dsets_[pframe] = dpool_.AllocateSet(device_, mesh_shader_program_.dset_layouts[0]);
  }
  DescriptorSetWriter dset_writer(mesh_shader_program_.dset_layout_cis[0]);
  for (uint32_t pframe = 0; pframe < PFRAME_COUNT; ++pframe) {
    dset_writer.BindBuffer(scene_uniforms_.Handle(pframe), mesh_vs_.GetDescriptorBindPoint("scene_consts").binding);
    dset_writer.BindBuffer(mesh_uniforms_.Handle(pframe), mesh_vs_.GetDescriptorBindPoint("mesh_consts").binding);
    dset_writer.WriteAll(device_, dsets_[pframe]);
  }

  // Create swapchain-sized buffers
  CreateRenderBuffers(swapchain_extent_);
}
CubeSwarmApp::~CubeSwarmApp() {
  if (device_ != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(device_);

    dpool_.Destroy(device_);

    mesh_uniforms_.Destroy(device_);
    scene_uniforms_.Destroy(device_);

    mesh_.Destroy(device_);

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

void CubeSwarmApp::Update(double dt) {
  seconds_elapsed_ += dt;
  drone_->Update(input_state_, (float)dt);
}

void CubeSwarmApp::Render(VkCommandBuffer primary_cb, uint32_t swapchain_image_index) {
  // Update uniforms
  SceneUniforms* uniforms = (SceneUniforms*)scene_uniforms_.Mapped(pframe_index_);
  uniforms->res_and_time =
      glm::vec4((float)swapchain_extent_.width, (float)swapchain_extent_.height, 0, (float)seconds_elapsed_);
  uniforms->eye = glm::vec4(camera_->getEyePoint(), 1.0f);
  glm::mat4 w2v = camera_->getViewMatrix();
  const glm::mat4 proj = camera_->getProjectionMatrix();
  uniforms->viewproj = proj * w2v;
  SPOKK_VK_CHECK(scene_uniforms_.FlushPframeHostCache(device_, pframe_index_));

  // Update object-to-world matrices.
  const float secs = (float)seconds_elapsed_;
  MeshUniforms* mesh_uniforms = (MeshUniforms*)mesh_uniforms_.Mapped(pframe_index_);
  const glm::vec3 swarm_center(0, 0, -2);
  for (uint32_t iMesh = 0; iMesh < MESH_INSTANCE_COUNT; ++iMesh) {
    // clang-format off
      mesh_uniforms->o2w[iMesh] = ComposeTransform(
        glm::vec3(
          40.0f * cosf(0.2f * secs + float(9*iMesh) + 0.4f) + swarm_center[0],
          20.5f * sinf(0.3f * secs + float(11*iMesh) + 5.0f) + swarm_center[1],
          30.0f * sinf(0.5f * secs + float(13*iMesh) + 2.0f) + swarm_center[2]),
        glm::angleAxis(
          secs + (float)iMesh,
          glm::normalize(glm::vec3(1,2,3))),
        3.0f);
    // clang-format on
  }
  SPOKK_VK_CHECK(mesh_uniforms_.FlushPframeHostCache(device_, pframe_index_));

  // Write command buffer
  VkFramebuffer framebuffer = framebuffers_[swapchain_image_index];
  render_pass_.begin_info.framebuffer = framebuffer;
  render_pass_.begin_info.renderArea.extent = swapchain_extent_;
  vkCmdBeginRenderPass(primary_cb, &render_pass_.begin_info, VK_SUBPASS_CONTENTS_INLINE);
  vkCmdBindPipeline(primary_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline_.handle);
  VkRect2D scissor_rect = render_pass_.begin_info.renderArea;
  VkViewport viewport = Rect2DToViewport(scissor_rect);
  vkCmdSetViewport(primary_cb, 0, 1, &viewport);
  vkCmdSetScissor(primary_cb, 0, 1, &scissor_rect);
  device_.DebugLabelInsert(primary_cb, "draw teapots");
  vkCmdBindDescriptorSets(primary_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline_.shader_program->pipeline_layout,
      0, 1, &dsets_[pframe_index_], 0, nullptr);
  mesh_.BindBuffers(primary_cb);
  vkCmdDrawIndexed(primary_cb, mesh_.index_count, MESH_INSTANCE_COUNT, 0, 0, 0);
  vkCmdEndRenderPass(primary_cb);
}

void CubeSwarmApp::HandleWindowResize(VkExtent2D new_window_extent) {
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

void CubeSwarmApp::CreateRenderBuffers(VkExtent2D extent) {
  // Create depth buffer
  VkImageCreateInfo depth_image_ci = render_pass_.GetAttachmentImageCreateInfo(1, extent);
  depth_image_ = {};
  SPOKK_VK_CHECK(depth_image_.Create(
      device_, depth_image_ci, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DEVICE_ALLOCATION_SCOPE_DEVICE));
  SPOKK_VK_CHECK(device_.SetObjectName(depth_image_.handle, "depth image"));
  SPOKK_VK_CHECK(device_.SetObjectName(depth_image_.view, "depth image view"));

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
    SPOKK_VK_CHECK(device_.SetObjectName(
        framebuffers_[i], std::string("swapchain framebuffer ") + std::to_string(i)));  // TODO(cort): absl::StrCat
  }
}

int main(int argc, char* argv[]) {
  (void)argc;
  (void)argv;

  std::vector<Application::QueueFamilyRequest> queue_requests = {
      {(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT), true, 1, 0.0f}};
  Application::CreateInfo app_ci = {};
  app_ci.queue_family_requests = queue_requests;
  app_ci.pfn_set_device_features = EnableMinimumDeviceFeatures;

  CubeSwarmApp app(app_ci);
  int run_error = app.Run();

  return run_error;
}
