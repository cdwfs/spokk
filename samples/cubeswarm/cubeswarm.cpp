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
constexpr uint32_t MESH_INSTANCE_COUNT = 1024;
constexpr uint32_t INDIRECT_DRAW_COUNT = 10 * MESH_INSTANCE_COUNT;
struct MeshUniforms {
  glm::mat4 o2w[MESH_INSTANCE_COUNT];
};
constexpr float FOV_DEGREES = 45.0f;
constexpr float Z_NEAR = 0.01f;
constexpr float Z_FAR = 100.0f;

enum TimestampId {
  TIMESTAMP_BEFORE_DRAW = 0,
  TIMESTAMP_AFTER_DRAW = 1,
  TIMESTAMP_COUNT,
};

}  // namespace

class CubeSwarmApp : public spokk::Application {
public:
  explicit CubeSwarmApp(Application::CreateInfo& ci) : Application(ci) {
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

    // Load textures and samplers
    VkSamplerCreateInfo sampler_ci =
        GetSamplerCreateInfo(VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);
    SPOKK_VK_CHECK(vkCreateSampler(device_, &sampler_ci, host_allocator_, &sampler_));
    albedo_tex_.CreateFromFile(device_, graphics_and_present_queue_, "data/redf.ktx");

    // Load shader pipelines
    SPOKK_VK_CHECK(mesh_vs_.CreateAndLoadSpirvFile(device_, "data/rigid_mesh.vert.spv"));
    SPOKK_VK_CHECK(mesh_fs_.CreateAndLoadSpirvFile(device_, "data/rigid_mesh.frag.spv"));
    SPOKK_VK_CHECK(mesh_shader_program_.AddShader(&mesh_vs_));
    SPOKK_VK_CHECK(mesh_shader_program_.AddShader(&mesh_fs_));
    SPOKK_VK_CHECK(mesh_shader_program_.Finalize(device_));

    // Populate Mesh object
    int mesh_load_error = mesh_.CreateFromFile(device_, "data/teapot.mesh");
    ZOMBO_ASSERT(!mesh_load_error, "load error: %d", mesh_load_error);

    // Create pipelined buffer of per-mesh object-to-world matrices.
    VkBufferCreateInfo o2w_buffer_ci = {};
    o2w_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    o2w_buffer_ci.size = MESH_INSTANCE_COUNT * sizeof(glm::mat4);
    o2w_buffer_ci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    o2w_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    SPOKK_VK_CHECK(mesh_uniforms_.Create(device_, PFRAME_COUNT, o2w_buffer_ci, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));

    // Create pipelined buffer of shader uniforms
    VkBufferCreateInfo scene_uniforms_ci = {};
    scene_uniforms_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    scene_uniforms_ci.size = sizeof(SceneUniforms);
    scene_uniforms_ci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    scene_uniforms_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    SPOKK_VK_CHECK(
        scene_uniforms_.Create(device_, PFRAME_COUNT, scene_uniforms_ci, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));

    // Create indirect draw parameter buffer
    VkBufferCreateInfo indirect_draw_buffers_ci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    indirect_draw_buffers_ci.size = INDIRECT_DRAW_COUNT * sizeof(VkDrawIndexedIndirectCommand);
    indirect_draw_buffers_ci.usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    indirect_draw_buffers_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    SPOKK_VK_CHECK(indirect_draw_buffers_.Create(
        device_, PFRAME_COUNT, indirect_draw_buffers_ci, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));

    mesh_pipeline_.Init(&mesh_.mesh_format, &mesh_shader_program_, &render_pass_, 0);
    SPOKK_VK_CHECK(mesh_pipeline_.Finalize(device_));

    for (const auto& dset_layout_ci : mesh_shader_program_.dset_layout_cis) {
      dpool_.Add(dset_layout_ci, PFRAME_COUNT);
    }
    SPOKK_VK_CHECK(dpool_.Finalize(device_));
    for (uint32_t pframe = 0; pframe < PFRAME_COUNT; ++pframe) {
      // TODO(cort): allocate_pipelined_set()?
      dsets_[pframe] = dpool_.AllocateSet(device_, mesh_shader_program_.dset_layouts[0]);
    }
    DescriptorSetWriter dset_writer(mesh_shader_program_.dset_layout_cis[0]);
    dset_writer.BindImage(
        albedo_tex_.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mesh_fs_.GetDescriptorBindPoint("tex").binding);
    dset_writer.BindSampler(sampler_, mesh_fs_.GetDescriptorBindPoint("samp").binding);
    for (uint32_t pframe = 0; pframe < PFRAME_COUNT; ++pframe) {
      dset_writer.BindBuffer(scene_uniforms_.Handle(pframe), mesh_vs_.GetDescriptorBindPoint("scene_consts").binding);
      dset_writer.BindBuffer(mesh_uniforms_.Handle(pframe), mesh_vs_.GetDescriptorBindPoint("mesh_consts").binding);
      dset_writer.WriteAll(device_, dsets_[pframe]);
    }

    TimestampQueryPool::CreateInfo tspool_ci = {};
    tspool_ci.swapchain_image_count = (uint32_t)swapchain_images_.size();
    tspool_ci.timestamp_id_count = TIMESTAMP_COUNT;
    tspool_ci.queue_family_index = graphics_and_present_queue_->family;
    SPOKK_VK_CHECK(timestamp_pool_.Create(device_, tspool_ci));

    // Create swapchain-sized buffers
    CreateRenderBuffers(swapchain_extent_);
  }
  virtual ~CubeSwarmApp() {
    if (device_ != VK_NULL_HANDLE) {
      vkDeviceWaitIdle(device_);

      timestamp_pool_.Destroy(device_);

      dpool_.Destroy(device_);

      indirect_draw_buffers_.Destroy(device_);
      mesh_uniforms_.Destroy(device_);
      scene_uniforms_.Destroy(device_);

      mesh_.Destroy(device_);

      mesh_vs_.Destroy(device_);
      mesh_fs_.Destroy(device_);
      mesh_shader_program_.Destroy(device_);
      mesh_pipeline_.Destroy(device_);

      vkDestroySampler(device_, sampler_, host_allocator_);
      albedo_tex_.Destroy(device_);

      for (const auto fb : framebuffers_) {
        vkDestroyFramebuffer(device_, fb, host_allocator_);
      }
      render_pass_.Destroy(device_);

      depth_image_.Destroy(device_);
    }
  }

  CubeSwarmApp(const CubeSwarmApp&) = delete;
  const CubeSwarmApp& operator=(const CubeSwarmApp&) = delete;

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
    scene_uniforms_.FlushPframeHostCache(pframe_index_);

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
        0.01f);//3.0f);
      // clang-format on
    }
    mesh_uniforms_.FlushPframeHostCache(pframe_index_);

    // Write indirect draw commands
    VkDrawIndexedIndirectCommand* indirect_draws =
        (VkDrawIndexedIndirectCommand*)indirect_draw_buffers_.Mapped(pframe_index_);
    memset(indirect_draws, 0, indirect_draw_buffers_.BytesPerPframe());
    for (uint32_t i = 0; i < MESH_INSTANCE_COUNT; ++i) {
      indirect_draws[i].indexCount = 3;  // mesh_.index_count;
      indirect_draws[i].instanceCount = 1;
      indirect_draws[i].firstIndex = 0;
      indirect_draws[i].vertexOffset = 0;
      indirect_draws[i].firstInstance = i;
    }
    indirect_draw_buffers_.FlushPframeHostCache(pframe_index_);

    // Retrieve earlier timestamps
    std::array<double, TIMESTAMP_COUNT> timestamps_seconds;
    std::array<bool, TIMESTAMP_COUNT> timestamps_validity;
    int64_t timestamps_frame_index = -1;
    SPOKK_VK_CHECK(timestamp_pool_.GetResults(device_, swapchain_image_index, TIMESTAMP_COUNT,
      timestamps_seconds.data(), timestamps_validity.data(), &timestamps_frame_index));
    if (timestamps_validity[TIMESTAMP_BEFORE_DRAW] && timestamps_validity[TIMESTAMP_AFTER_DRAW]) {
      float draw_time_ms = 1000.0f *
        (float)(timestamps_seconds[TIMESTAMP_AFTER_DRAW] - timestamps_seconds[TIMESTAMP_BEFORE_DRAW]);
      const uint32_t gpu_stats_frame_index = (uint32_t)(timestamps_frame_index % FRAME_TIME_COUNT);
      gpu_draw_times_average_ms_ +=
        (draw_time_ms - gpu_draw_times_ms_[gpu_stats_frame_index]) / (float)FRAME_TIME_COUNT;
      gpu_draw_times_ms_[gpu_stats_frame_index] = draw_time_ms;
      if (gpu_stats_frame_index == FRAME_TIME_COUNT-1) {
        printf("avg: %.3fms\n", gpu_draw_times_average_ms_);
      }
    }

    // Write command buffer
    timestamp_pool_.SetTargetFrame(primary_cb, swapchain_image_index, frame_index_);
    VkFramebuffer framebuffer = framebuffers_[swapchain_image_index];
    render_pass_.begin_info.framebuffer = framebuffer;
    render_pass_.begin_info.renderArea.extent = swapchain_extent_;
    vkCmdBeginRenderPass(primary_cb, &render_pass_.begin_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(primary_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline_.handle);
    VkRect2D scissor_rect = render_pass_.begin_info.renderArea;
    VkViewport viewport = Rect2DToViewport(scissor_rect);
    vkCmdSetViewport(primary_cb, 0, 1, &viewport);
    vkCmdSetScissor(primary_cb, 0, 1, &scissor_rect);
    mesh_.BindBuffers(primary_cb);
    vkCmdBindDescriptorSets(primary_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline_.shader_program->pipeline_layout,
        0, 1, &dsets_[pframe_index_], 0, nullptr);
    timestamp_pool_.WriteTimestamp(primary_cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, TIMESTAMP_BEFORE_DRAW);
#if 1
    // One instanced draw call
    vkCmdDrawIndexed(primary_cb, 3, MESH_INSTANCE_COUNT, 0, 0, 0);
#elif 1
    // One draw call per instance
    for (uint32_t i = 0; i < MESH_INSTANCE_COUNT; ++i) {
      vkCmdDrawIndexed(primary_cb, 3, 1, 0, 0, i);
    }
#elif 1
    // Multi draw indirect
    vkCmdDrawIndexedIndirect(primary_cb, indirect_draw_buffers_.Handle(pframe_index_), 0, MESH_INSTANCE_COUNT,
        sizeof(VkDrawIndexedIndirectCommand));
#elif 1
    // One indirect draw call per instance
    for (uint32_t i = 0; i < MESH_INSTANCE_COUNT; ++i) {
      vkCmdDrawIndexedIndirect(primary_cb, indirect_draw_buffers_.Handle(pframe_index_),
          i * sizeof(VkDrawIndexedIndirectCommand), 1, sizeof(VkDrawIndexedIndirectCommand));
    }
#elif 1
    // Sparse Multi draw indirect
    vkCmdDrawIndexedIndirect(primary_cb, indirect_draw_buffers_.Handle(pframe_index_), 0, INDIRECT_DRAW_COUNT,
        sizeof(VkDrawIndexedIndirectCommand));
#endif
    timestamp_pool_.WriteTimestamp(primary_cb, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TIMESTAMP_AFTER_DRAW);
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

  Image albedo_tex_;
  VkSampler sampler_;

  Shader mesh_vs_, mesh_fs_;
  ShaderProgram mesh_shader_program_;
  GraphicsPipeline mesh_pipeline_;

  DescriptorPool dpool_;
  std::array<VkDescriptorSet, PFRAME_COUNT> dsets_;

  Mesh mesh_;
  PipelinedBuffer mesh_uniforms_;
  PipelinedBuffer scene_uniforms_;

  PipelinedBuffer indirect_draw_buffers_;

  TimestampQueryPool timestamp_pool_;
  static constexpr size_t FRAME_TIME_COUNT = 100;
  std::array<float, FRAME_TIME_COUNT> gpu_draw_times_ms_ = {};
  float gpu_draw_times_average_ms_ = 0.0f;

  std::unique_ptr<CameraPersp> camera_;
  std::unique_ptr<CameraDrone> drone_;
};

static VkBool32 EnableDeviceFeatures(
    const VkPhysicalDeviceFeatures& supported_features, VkPhysicalDeviceFeatures* enabled_features) {
  if (!EnableMinimumDeviceFeatures(supported_features, enabled_features)) {
    return false;
  }

  if (!supported_features.multiDrawIndirect) {
    return VK_FALSE;
  }
  enabled_features->multiDrawIndirect = VK_TRUE;

  if (!supported_features.drawIndirectFirstInstance) {
    return VK_FALSE;
  }
  enabled_features->drawIndirectFirstInstance = VK_TRUE;

  return VK_TRUE;
}

int main(int argc, char* argv[]) {
  (void)argc;
  (void)argv;

  std::vector<Application::QueueFamilyRequest> queue_requests = {
      {(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT), true, 1, 0.0f}};
  Application::CreateInfo app_ci = {};
  app_ci.queue_family_requests = queue_requests;
  app_ci.pfn_set_device_features = EnableDeviceFeatures;
  CubeSwarmApp app(app_ci);
  int run_error = app.Run();

  return run_error;
}
